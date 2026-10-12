// SPDX-License-Identifier: GPL-2.0-or-later
#include "folder-dock.hpp"

#include <util/config-file.h>
#include <QApplication>
#include <QMenu>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {
struct Destination {
	const char *section;
	const char *key;
	bool local;
};

QString value(config_t *config, const char *section, const char *key)
{
	const char *text = config_get_string(config, section, key);
	return QString::fromUtf8(text ? text : "");
}

Destination destination(config_t *config)
{
	if (value(config, "Output", "Mode").compare("Advanced", Qt::CaseInsensitive) != 0)
		return {"SimpleOutput", "FilePath", true};
	if (value(config, "AdvOut", "RecType").compare("FFmpeg", Qt::CaseInsensitive) == 0)
		return {"AdvOut", "FFFilePath", config_get_bool(config, "AdvOut", "FFOutputToFile")};
	return {"AdvOut", "RecFilePath", true};
}
} // namespace

FolderDock::FolderDock(QWidget *parent, const QString &settingsFile) : QWidget(parent)
{
	if (!settingsFile.isEmpty())
		settings_ = std::make_unique<QSettings>(settingsFile, QSettings::IniFormat);
	setMinimumWidth(250);
	auto *layout = new QVBoxLayout(this);
	auto *label = new QLabel(tr("Recording folder"), this);
	layout->addWidget(label);
	path_ = new QLineEdit(this);
	path_->setObjectName("recordingFolderPath");
	path_->setReadOnly(true);
	path_->setAccessibleName(tr("Current recording folder"));
	label->setBuddy(path_);
	layout->addWidget(path_);
	auto *buttons = new QHBoxLayout;
	choose_ = new QPushButton(tr("Choose folder..."), this);
	choose_->setObjectName("chooseFolder");
	open_ = new QPushButton(tr("Open folder"), this);
	buttons->addWidget(choose_);
	buttons->addWidget(open_);
	layout->addLayout(buttons);
	editPresets_ = new QPushButton(tr("Presets..."), this);
	editPresets_->setObjectName("editPresets");
	recents_ = new QPushButton(tr("Recents"), this);
	recents_->setObjectName("recents");
	recentMenu_ = new QMenu(recents_);
	recents_->setMenu(recentMenu_);
	auto *locationsRow = new QHBoxLayout;
	locationsRow->addWidget(editPresets_);
	locationsRow->addWidget(recents_);
	layout->addLayout(locationsRow);
	connect(editPresets_, &QPushButton::clicked, this, &FolderDock::editPresets);
	connect(recentMenu_, &QMenu::aboutToShow, this, &FolderDock::updateLocations);
	automatic_ = new QPushButton(tr("Auto-stop / restart replay"), this);
	automatic_->setObjectName("automaticFolderChange");
	automatic_->setCheckable(true);
	automatic_->setToolTip(tr("Choose folder stops recording and replay. After a folder is saved, replay starts again. Unsaved replay footage is discarded. Cancel leaves outputs stopped."));
	layout->addWidget(automatic_);
	connect(automatic_, &QPushButton::toggled, this, [this] { notice_.clear(); saveLocations(); refresh(); });
	stopTimeout_ = new QTimer(this);
	stopTimeout_->setSingleShot(true);
	stopTimeout_->setInterval(30000);
	connect(stopTimeout_, &QTimer::timeout, this, [this] {
		waitingForStop_ = false;
		automaticChange_ = false;
		notice_ = tr("OBS has not finished stopping. No folder was changed; try again once outputs stop.");
		refresh();
	});
	status_ = new QLabel(this);
	status_->setWordWrap(true);
	layout->addWidget(status_);
	layout->addStretch();
	loadLocations();
	connect(choose_, &QPushButton::clicked, this, [this] { chooseFolder(); });
	connect(open_, &QPushButton::clicked, this, [this] {
		if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path_->text())))
			QMessageBox::warning(this, tr("Recording Folder"), tr("Could not open this folder."));
	});
	recordingBusy_ = obs_frontend_recording_active();
	replayBusy_ = obs_frontend_replay_buffer_active();
	obs_frontend_add_event_callback(onEvent, this);
	// OBS has no frontend event for Output Settings being applied.
	auto *timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, &FolderDock::refresh);
	timer->start(1000);
	refresh();
}

FolderDock::~FolderDock()
{
	obs_frontend_remove_event_callback(onEvent, this);
	// Close dialogs while our state still exists; their destruction callbacks refresh the dock.
	if (picker_) {
		disconnect(picker_, nullptr, this, nullptr);
		delete picker_.data();
	}
	if (editor_) {
		disconnect(editor_, nullptr, this, nullptr);
		delete editor_.data();
	}
}

QString FolderDock::unavailableReason(bool ignoreOutputs) const
{
	if (shuttingDown_ || profileChanging_)
		return tr("Waiting for OBS...");
	if (!ignoreOutputs && (recordingBusy_ || obs_frontend_recording_active()))
		return tr("Stop recording before changing the folder.");
	if (!ignoreOutputs && (replayBusy_ || obs_frontend_replay_buffer_active()))
		return tr("Stop the replay buffer before changing the folder.");
	auto *config = obs_frontend_get_profile_config();
	if (!config)
		return tr("No OBS profile is available.");
	if (!destination(config).local)
		return tr("Custom FFmpeg output is using a URL. Select output to a file in OBS Settings first.");
	if (auto *modal = QApplication::activeModalWidget(); modal && modal != picker_.data())
		return tr("Close the OBS dialog before changing the folder.");
	return {};
}

void FolderDock::refresh()
{
	if (shuttingDown_ || profileChanging_)
		return;
	QString folder;
	if (auto *config = obs_frontend_get_profile_config()) {
		const auto target = destination(config);
		if (target.local)
			folder = QDir::toNativeSeparators(value(config, target.section, target.key));
	}
	if (path_->text() != folder) {
		path_->setText(folder);
		path_->setCursorPosition(0);
		path_->setToolTip(folder);
	}
	const QString reason = unavailableReason(automatic_->isChecked());
	choose_->setEnabled(reason.isEmpty() && !picker_ && !waitingForStop_);
	editPresets_->setEnabled(!picker_ && !waitingForStop_ && !editor_);
	recents_->setEnabled(!picker_ && !waitingForStop_ && !editor_);
	automatic_->setEnabled(!picker_ && !waitingForStop_);
	open_->setEnabled(!folder.isEmpty() && QFileInfo(folder).isDir());
	status_->setText(waitingForStop_ ? tr("Waiting for recording and replay to finish stopping...") :
		!notice_.isEmpty() ? notice_ : !reason.isEmpty() ? reason : automatic_->isChecked() ?
		tr("Choose folder stops recording and replay; saving a folder starts replay. Unsaved replay footage is discarded.") :
		tr("Applies to the next recording in this OBS profile."));
}

bool FolderDock::applyFolder(const QString &folder, QString &error)
{
	error = unavailableReason();
	if (!error.isEmpty())
		return false;
	const QFileInfo info(folder);
	if (folder.isEmpty() || !info.isAbsolute() || !info.exists() || !info.isDir()) {
		error = tr("Choose an existing folder using an absolute path.");
		return false;
	}
	const QString normalized = QDir::cleanPath(info.absoluteFilePath());
	// Test actual write access, including ACLs and network shares. Auto-removes the probe.
	QTemporaryFile probe(QDir(normalized).filePath(".obs-folder-write-test-XXXXXX"));
	if (!probe.open() || probe.write("obs", 3) != 3 || !probe.flush()) {
		error = tr("OBS cannot write to this folder. Choose another folder or check its permissions.");
		return false;
	}
	probe.close();
	auto *config = obs_frontend_get_profile_config();
	const auto target = destination(config);
	const bool hadValue = config_has_user_value(config, target.section, target.key);
	const QByteArray previous = value(config, target.section, target.key).toUtf8();
	const QByteArray encoded = QDir::toNativeSeparators(normalized).toUtf8();
	config_set_string(config, target.section, target.key, encoded.constData());
	if (config_save_safe(config, "tmp", "bak") != CONFIG_SUCCESS) {
		if (hadValue)
			config_set_string(config, target.section, target.key, previous.constData());
		else
			config_remove_value(config, target.section, target.key);
		error = tr("Could not save the OBS profile. The recording folder has not been changed.");
		return false;
	}
	rememberFolder(normalized);
	refresh();
	return true;
}

void FolderDock::chooseFolder(const QString &folder)
{
	if (!unavailableReason(automatic_->isChecked()).isEmpty() || picker_ || waitingForStop_)
		return;
	notice_.clear();
	if (!folder.isEmpty() && (!QFileInfo(folder).isAbsolute() || !QFileInfo(folder).isDir())) {
		notice_ = tr("That folder is unavailable. Edit the preset or choose an existing folder.");
		refresh();
		return;
	}
	pendingFolder_ = folder;
	automaticChange_ = automatic_->isChecked();
	if (!automaticChange_) {
		selectDestination();
		return;
	}
	waitingForStop_ = true;
	stopRecordingRequested_ = false;
	stopReplayRequested_ = false;
	stopTimeout_->start();
	// Stop requests are asynchronous. Only open the picker after both STOPPED events.
	continueFolderChange();
	refresh();
}

void FolderDock::continueFolderChange()
{
	if (!waitingForStop_)
		return;
	// A second stop request during finalization can force-stop/truncate a recording.
	if (!stopRecordingRequested_ && !recordingStopping_ && obs_frontend_recording_active()) {
		stopRecordingRequested_ = true;
		obs_frontend_recording_stop();
	}
	if (!stopReplayRequested_ && !replayStopping_ && obs_frontend_replay_buffer_active()) {
		stopReplayRequested_ = true;
		obs_frontend_replay_buffer_stop();
	}
	if (recordingBusy_ || replayBusy_ ||
	    obs_frontend_recording_active() || obs_frontend_replay_buffer_active())
		return;
	waitingForStop_ = false;
	stopTimeout_->stop();
	if (!unavailableReason().isEmpty()) {
		automaticChange_ = false;
		refresh();
		return;
	}
	selectDestination();
}

void FolderDock::selectDestination()
{
	if (pendingFolder_.isEmpty())
		openPicker();
	else {
		const QString folder = pendingFolder_;
		pendingFolder_.clear();
		finishSelection(folder);
	}
}

void FolderDock::finishSelection(const QString &folder)
{
	QString error;
	if (!applyFolder(folder, error)) {
		notice_ = error;
	} else if (automaticChange_) {
		obs_frontend_replay_buffer_start();
		if (notice_.isEmpty())
			notice_ = tr("Folder saved. Replay start requested; recording remains stopped.");
	}
	refresh();
}

void FolderDock::openPicker()
{
	if (!unavailableReason().isEmpty() || picker_)
		return;
	const auto revision = profileRevision_;
	auto *config = obs_frontend_get_profile_config();
	const auto target = destination(config);
	const QString original = value(config, target.section, target.key);
	auto *dialog = new QFileDialog(this, tr("Choose recording folder"), original);
	picker_ = dialog;
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->setFileMode(QFileDialog::Directory);
	dialog->setOption(QFileDialog::ShowDirsOnly);
	// Use the platform's native folder picker (Windows Explorer on Windows).
	connect(dialog, &QFileDialog::accepted, this, [this, dialog, revision, target, original] {
		if (shuttingDown_ || revision != profileRevision_)
			return;
		auto *currentConfig = obs_frontend_get_profile_config();
		if (!currentConfig)
			return;
		const auto current = destination(currentConfig);
		if (QString::fromUtf8(current.key) != QString::fromUtf8(target.key) ||
		    value(currentConfig, current.section, current.key) != original) {
			QMessageBox::information(this, tr("Recording Folder"),
					 tr("OBS output settings changed while choosing a folder. Please choose again."));
			return;
		}
		const auto selected = dialog->selectedFiles();
		if (!selected.isEmpty())
			finishSelection(selected.first());
	});
	connect(dialog, &QFileDialog::rejected, this, [this] {
		if (automaticChange_)
			notice_ = tr("Folder selection canceled. Recording and replay remain stopped.");
	});
	connect(dialog, &QObject::destroyed, this, [this] { refresh(); });
	dialog->open();
	refresh();
}

void FolderDock::onEvent(obs_frontend_event event, void *data)
{
	auto *dock = static_cast<FolderDock *>(data);
	switch (event) {
	case OBS_FRONTEND_EVENT_RECORDING_STARTING:
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		dock->recordingStopping_ = false;
		dock->recordingBusy_ = true;
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPING:
		dock->recordingStopping_ = true;
		dock->recordingBusy_ = true;
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		dock->recordingStopping_ = false;
		dock->recordingBusy_ = false;
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING:
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED:
		dock->replayStopping_ = false;
		dock->replayBusy_ = true;
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPING:
		dock->replayStopping_ = true;
		dock->replayBusy_ = true;
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
		dock->replayStopping_ = false;
		dock->replayBusy_ = false;
		break;
	case OBS_FRONTEND_EVENT_PROFILE_CHANGING:
		dock->waitingForStop_ = false;
		dock->automaticChange_ = false;
		dock->stopTimeout_->stop();
		dock->notice_.clear();
		dock->profileChanging_ = true;
		++dock->profileRevision_;
		if (dock->picker_)
			dock->picker_->reject();
		break;
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
		dock->profileChanging_ = false;
		++dock->profileRevision_;
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		dock->waitingForStop_ = false;
		dock->automaticChange_ = false;
		dock->stopTimeout_->stop();
		dock->shuttingDown_ = true;
		if (dock->picker_)
			dock->picker_->reject();
		break;
	default:
		break;
	}
	dock->refresh();
	if (dock->waitingForStop_)
		QTimer::singleShot(0, dock, &FolderDock::continueFolderChange);
}

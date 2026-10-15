// SPDX-License-Identifier: GPL-2.0-or-later
#include "folder-dock.hpp"

#include <util/config-file.h>
#include <QApplication>
#include <QMenu>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <obs.h>
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

FolderDock::FolderDock(QWidget *parent, const QString &settingsFile, std::function<qint64()> clock, bool synchronousChecks)
	: QWidget(parent), clock_(std::move(clock)), synchronousChecks_(synchronousChecks)
{
	if (!settingsFile.isEmpty())
		settings_ = std::make_unique<QSettings>(settingsFile, QSettings::IniFormat);
	lifetime_.start();
	healthPool_.setMaxThreadCount(1);
	setMinimumWidth(250);
	folderWatcher_ = new QFileSystemWatcher(this);
	connect(folderWatcher_, &QFileSystemWatcher::directoryChanged, this, [this] { nextHealthCheck_ = 0; refresh(); });
	auto *layout = new QVBoxLayout(this);
	path_ = new QLineEdit(this);
	path_->setObjectName("recordingFolderPath");
	path_->setReadOnly(true);
	path_->setAccessibleName(tr("Current recording folder"));
	auto *pathRow = new QHBoxLayout;
	pathRow->addWidget(path_, 1);
	folderAccess_ = new QLabel(this);
	folderAccess_->setObjectName("folderAccess");
	folderAccess_->setAlignment(Qt::AlignCenter);
	folderAccess_->setMinimumWidth(24);
	pathRow->addWidget(folderAccess_);
	layout->addLayout(pathRow);
	auto *buttons = new QHBoxLayout;
	choose_ = new QPushButton(tr("Choose folder..."), this);
	choose_->setObjectName("chooseFolder");
	open_ = new QPushButton(tr("Open folder"), this);
	buttons->addWidget(choose_);
	buttons->addWidget(open_);
	layout->addLayout(buttons);
	presets_ = new QPushButton(tr("Presets"), this);
	presets_->setObjectName("presets");
	presetMenu_ = new QMenu(presets_);
	presets_->setMenu(presetMenu_);
	editPresets_ = new QPushButton(tr("+"), this);
	editPresets_->setObjectName("editPresets");
	editPresets_->setAccessibleName(tr("Configure presets"));
	editPresets_->setToolTip(tr("Add or edit preset folders"));
	// Let the active OBS theme include its padding in the button's size hint.
	editPresets_->setMinimumWidth(40);
	editPresets_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	recents_ = new QPushButton(tr("Recents"), this);
	recents_->setObjectName("recents");
	recentMenu_ = new QMenu(recents_);
	recents_->setMenu(recentMenu_);
	auto *locationsRow = new QHBoxLayout;
	locationsRow->addWidget(presets_, 1);
	locationsRow->addWidget(editPresets_);
	layout->addLayout(locationsRow);
	layout->addWidget(recents_);
	connect(editPresets_, &QPushButton::clicked, this, &FolderDock::editPresets);
	connect(recentMenu_, &QMenu::aboutToShow, this, &FolderDock::updateLocations);
	connect(presetMenu_, &QMenu::aboutToShow, this, &FolderDock::updatePresetMenu);
	automatic_ = new QPushButton(tr("Auto-stop / restart replay"), this);
	automatic_->setObjectName("automaticFolderChange");
	automatic_->setCheckable(true);
	automatic_->setToolTip(tr("Choose folder stops recording and replay. Selecting a folder or canceling restarts replay. Recording stays stopped. Unsaved replay footage is discarded."));
	layout->addWidget(automatic_);
	connect(automatic_, &QPushButton::toggled, this, [this] { notice_.clear(); saveLocations(); refresh(); });
	changePoll_ = new QTimer(this);
	changePoll_->setInterval(250);
	changePoll_->setObjectName("operationPoll");
	connect(changePoll_, &QTimer::timeout, this, [this] {
		if (operation_ == Operation::Stopping && now() - stopStartedAt_ >= 30000) {
			operation_ = Operation::Idle;
			automaticChange_ = false;
			blog(LOG_WARNING, "[recording-folder-dock] Output stop timed out");
			notice_ = tr("OBS has not finished stopping. No folder was changed; try again once outputs stop.");
		} else {
			continueFolderChange();
		}
		continueReplayRestart();
		if (operation_ != Operation::Stopping && operation_ != Operation::Restarting) changePoll_->stop();
		refresh();
	});
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
	shuttingDown_ = true;
	if (healthCancel_) healthCancel_->store(true);
	healthPool_.waitForDone(); // Drain plugin code before OBS unloads the module.
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
	maintainPresetFolder(folder);
	if (path_->text() != folder) {
		path_->setText(folder);
		path_->setCursorPosition(0);
		path_->setToolTip(folder);
	}
	const QString reason = unavailableReason(automatic_->isChecked());
	choose_->setEnabled(reason.isEmpty() && !picker_ && operation_ == Operation::Idle);
	editPresets_->setEnabled(!picker_ && operation_ == Operation::Idle && !editor_);
	recents_->setEnabled(!picker_ && operation_ == Operation::Idle && !editor_);
	presets_->setEnabled(!picker_ && operation_ == Operation::Idle && !editor_);
	automatic_->setEnabled(!picker_ && operation_ == Operation::Idle);
	const bool accessible = normalizedFolder(health_.path) == normalizedFolder(folder) && health_.accessible;
	if (folderAccess_->property("available").isNull() || folderAccess_->property("available").toBool() != accessible) {
		folderAccess_->setProperty("available", accessible);
		folderAccess_->setText(accessible ? QString(QChar(0x2713)) : QString(QChar(0x00D7)));
		folderAccess_->setStyleSheet(accessible ? "color: #28a745; font-size: 20px; font-weight: bold;" :
		"color: #e55353; font-size: 20px; font-weight: bold;");
	}
	const QString accessDescription = accessible ? tr("Folder available (read/write access reported).") :
		tr("Folder unavailable or access is restricted.");
	folderAccess_->setToolTip(accessDescription);
	folderAccess_->setAccessibleName(accessDescription);
	open_->setEnabled(normalizedFolder(health_.path) == normalizedFolder(folder) && health_.exists);
	setToolTip(!notice_.isEmpty() ? notice_ : reason);
}

qint64 FolderDock::now() const
{
	return clock_ ? clock_() : lifetime_.elapsed();
}

void FolderDock::maintainPresetFolder(const QString &folder)
{
	if (healthPending_) {
		if (healthPath_ != folder && healthCancel_) healthCancel_->store(true);
		return;
	}
	if (!synchronousChecks_ && healthPath_ == folder && now() < nextHealthCheck_) return;
	if (healthPath_ != folder) healthFailures_ = 0;
	healthPath_ = folder;
	bool preset = false;
	for (const auto &location : locations_)
		if (normalizedFolder(location.second) == normalizedFolder(folder)) preset = true;
	const auto expected = volumes_.value(normalizedFolder(folder));
	if (synchronousChecks_) {
		acceptFolderHealth(checkFolder(folder, preset, expected), profileRevision_);
	} else {
		healthPending_ = true;
		healthCancel_ = std::make_shared<std::atomic_bool>(false);
		const auto cancel = healthCancel_;
		const auto revision = profileRevision_;
		// The destructor drains this private pool before QObject or the module can disappear.
		healthPool_.start([this, folder, preset, expected, revision, cancel] {
			const auto result = checkFolder(folder, preset, expected, cancel);
			QMetaObject::invokeMethod(this, [this, result, revision] {
				healthPending_ = false;
				acceptFolderHealth(result, revision);
				refresh();
			}, Qt::QueuedConnection);
		});
	}
}

void FolderDock::acceptFolderHealth(const FolderHealth &health, unsigned long long revision)
{
	if (shuttingDown_ || profileChanging_ || revision != profileRevision_ || health.canceled) {
		nextHealthCheck_ = 0;
		return;
	}
	auto *config = obs_frontend_get_profile_config();
	if (!config || normalizedFolder(value(config, destination(config).section, destination(config).key)) != normalizedFolder(health.path)) return;
	const bool changedPath = normalizedFolder(health_.path) != normalizedFolder(health.path);
	health_ = health;
	healthFailures_ = health.error.isEmpty() ? 0 : qMin(healthFailures_ + 1, 5);
	nextHealthCheck_ = now() + (health.error.isEmpty() ? 1000 : (1000 << healthFailures_));
	if (!health.error.isEmpty()) {
		if (recoveryError_ != health.error || changedPath)
			blog(LOG_WARNING, "[recording-folder-dock] Folder check: %s (%s)", qUtf8Printable(health.error), qUtf8Printable(health.path));
		recoveryError_ = health.error;
		notice_ = recoveryError_;
	} else {
		if (notice_ == recoveryError_) notice_.clear();
		recoveryError_.clear();
		if (health.recovered) blog(LOG_INFO, "[recording-folder-dock] Recreated active preset folder: %s", qUtf8Printable(health.path));
		for (const auto &location : locations_) {
			const auto key = normalizedFolder(location.second);
			if (key == normalizedFolder(health.path) && !volumes_.value(key).known() && health.volume.known()) {
				volumes_.insert(key, health.volume);
				saveLocations();
			}
		}
	}
	// Remote drives use asynchronous polling, avoiding GUI-thread network watch setup.
	QStringList watches;
	if (health.exists && health.localWatch)
		watches = {health.path, health.parent};
	const auto current = folderWatcher_->directories();
	for (const auto &path : current) if (!watches.contains(path)) folderWatcher_->removePath(path);
	for (const auto &path : watches) if (!current.contains(path)) folderWatcher_->addPath(path);
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
		blog(LOG_WARNING, "[recording-folder-dock] Failed to save recording destination; restored previous setting");
		error = tr("Could not save the OBS profile. The recording folder has not been changed.");
		return false;
	}
	for (const auto &location : locations_)
		if (normalizedFolder(location.second) == normalizedFolder(normalized))
			volumes_.insert(normalizedFolder(normalized), folderVolume(normalized));
	rememberFolder(normalized);
	nextHealthCheck_ = 0;
	refresh();
	return true;
}

void FolderDock::chooseFolder(const QString &folder, bool createMissing)
{
	if (!unavailableReason(automatic_->isChecked()).isEmpty() || picker_ || operation_ != Operation::Idle)
		return;
	notice_.clear();
	if (createMissing) {
		const auto expected = volumes_.value(normalizedFolder(folder));
		if (expected.known() && !(folderVolume(folder) == expected)) {
			notice_ = tr("The preset's original drive is unavailable. Reconnect it before selecting this preset.");
			blog(LOG_WARNING, "[recording-folder-dock] Preset volume mismatch: %s", qUtf8Printable(folder));
			refresh();
			return;
		}
	}
	if (createMissing && QFileInfo(folder).isAbsolute() && !QFileInfo(folder).isDir()) {
		if (!QDir().mkpath(folder)) {
			notice_ = tr("Could not recreate preset folder: %1. Check the path, drive availability, and permissions.")
				.arg(QDir::toNativeSeparators(folder));
			refresh();
			return;
		}
	}
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
	operation_ = Operation::Stopping;
	stopRecordingRequested_ = false;
	stopReplayRequested_ = false;
	stopStartedAt_ = now();
	changePoll_->start();
	// Stop requests are asynchronous. Only open the picker after both STOPPED events.
	continueFolderChange();
	refresh();
}

void FolderDock::continueFolderChange()
{
	if (operation_ != Operation::Stopping)
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
	operation_ = Operation::Idle;
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
		beginReplayRestart();
	}
	automaticChange_ = false;
	refresh();
}

void FolderDock::beginReplayRestart(bool defer)
{
	auto *config = obs_frontend_get_profile_config();
	if (!config || shuttingDown_ || profileChanging_) return;
	const auto target = destination(config);
	if (!target.local) return;
	operation_ = Operation::Restarting;
	restartAttempts_ = 0;
	restartRevision_ = profileRevision_;
	restartFolder_ = value(config, target.section, target.key);
	restartKey_ = QString::fromUtf8(target.key);
	restartStartedAt_ = now();
	changePoll_->start();
	if (defer)
		QTimer::singleShot(0, this, &FolderDock::continueReplayRestart);
	else
		continueReplayRestart();
}

void FolderDock::continueReplayRestart()
{
	if (operation_ != Operation::Restarting) return;
	auto *config = obs_frontend_get_profile_config();
	if (shuttingDown_ || profileChanging_ || restartRevision_ != profileRevision_ || !config ||
	    QString::fromUtf8(destination(config).key) != restartKey_ ||
	    QDir::fromNativeSeparators(value(config, destination(config).section, destination(config).key)) !=
	    QDir::fromNativeSeparators(restartFolder_)) {
		operation_ = Operation::Idle;
		return;
	}
	if (obs_frontend_replay_buffer_active()) {
		blog(LOG_INFO, "[recording-folder-dock] Replay restart confirmed");
		operation_ = Operation::Idle;
		return;
	}
	if ((now() - restartStartedAt_) >= 10000) {
		operation_ = Operation::Idle;
		// STARTING can be followed by a failed OBS start with no STOPPED event.
		if (!obs_frontend_replay_buffer_active()) { replayBusy_ = false; replayStopping_ = false; }
		blog(LOG_WARNING, "[recording-folder-dock] Replay restart timed out after %d attempts", restartAttempts_);
		notice_ = tr("Replay did not restart. Check OBS replay settings and any output error.");
		return;
	}
	// STARTING means OBS accepted the request. Do not duplicate an in-flight start.
	if (replayBusy_ || replayStopping_ || recordingBusy_ || obs_frontend_recording_active()) return;
	if (!unavailableReason().isEmpty()) return;
	if (restartAttempts_ < 3 && (now() - restartStartedAt_) >= restartAttempts_ * 1500) {
		++restartAttempts_;
		blog(LOG_INFO, "[recording-folder-dock] Replay restart attempt %d", restartAttempts_);
		obs_frontend_replay_buffer_start();
	}
}

void FolderDock::openPicker()
{
	if (!unavailableReason().isEmpty() || picker_)
		return;
	const auto revision = profileRevision_;
	auto *config = obs_frontend_get_profile_config();
	const auto target = destination(config);
	const QString original = value(config, target.section, target.key);
	operation_ = Operation::Choosing;
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
	connect(dialog, &QFileDialog::rejected, this, [this, revision, target, original] {
		const bool resume = automaticChange_;
		automaticChange_ = false;
		auto *config = obs_frontend_get_profile_config();
		if (!resume || shuttingDown_ || profileChanging_ || revision != profileRevision_ || !config) return;
		const auto current = destination(config);
		if (QString::fromUtf8(current.key) != QString::fromUtf8(target.key) ||
		    value(config, current.section, current.key) != original) return;
		// Let the native dialog finish closing before requesting output startup.
		beginReplayRestart(true);
		refresh();
	});
	connect(dialog, &QObject::destroyed, this, [this] {
		if (operation_ == Operation::Choosing) operation_ = Operation::Idle;
		refresh();
	});
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
		if (event == OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED && dock->operation_ == Operation::Restarting) {
			blog(LOG_INFO, "[recording-folder-dock] Replay restart confirmed");
			dock->operation_ = Operation::Idle;
		}
		dock->replayStopping_ = false;
		dock->replayBusy_ = true;
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPING:
		if (dock->operation_ == Operation::Restarting) dock->operation_ = Operation::Idle;
		dock->replayStopping_ = true;
		dock->replayBusy_ = true;
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
		dock->replayStopping_ = false;
		dock->replayBusy_ = false;
		break;
	case OBS_FRONTEND_EVENT_PROFILE_CHANGING:
		if (dock->healthCancel_) dock->healthCancel_->store(true);
		dock->operation_ = Operation::Idle;
		dock->automaticChange_ = false;
		dock->notice_.clear();
		dock->profileChanging_ = true;
		++dock->profileRevision_;
		if (dock->picker_)
			dock->picker_->reject();
		break;
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
		dock->nextHealthCheck_ = 0;
		dock->profileChanging_ = false;
		++dock->profileRevision_;
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		dock->operation_ = Operation::Idle;
		dock->automaticChange_ = false;
		dock->shuttingDown_ = true;
		if (dock->picker_)
			dock->picker_->reject();
		break;
	default:
		break;
	}
	dock->refresh();
	if (dock->operation_ == Operation::Stopping)
		QTimer::singleShot(0, dock, &FolderDock::continueFolderChange);
}

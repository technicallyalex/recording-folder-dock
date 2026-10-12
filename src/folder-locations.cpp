// SPDX-License-Identifier: GPL-2.0-or-later
#include "folder-dock.hpp"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>

void FolderDock::loadLocations()
{
	if (settings_) {
		QSignalBlocker block(automatic_);
		automatic_->setChecked(settings_->value("autoStop", false).toBool());
		recentPaths_ = settings_->value("recentPaths").toStringList();
		recentPaths_.removeDuplicates();
		while (recentPaths_.size() > 10)
			recentPaths_.removeLast();
		const int count = settings_->beginReadArray("presets");
		for (int index = 0; index < count && index < 50; ++index) {
			settings_->setArrayIndex(index);
			const auto name = settings_->value("name").toString();
			const auto path = settings_->value("path").toString();
			if (!name.isEmpty() && !path.isEmpty())
				locations_.append({name, path});
		}
		settings_->endArray();
		if (settings_->status() != QSettings::NoError)
			notice_ = tr("Could not read saved dock preferences.");
	}
	updateLocations();
}

bool FolderDock::saveLocations()
{
	if (!settings_)
		return true;
	settings_->setValue("autoStop", automatic_->isChecked());
	settings_->setValue("recentPaths", recentPaths_);
	settings_->beginWriteArray("presets", static_cast<int>(locations_.size()));
	for (int index = 0; index < locations_.size(); ++index) {
		settings_->setArrayIndex(index);
		settings_->setValue("name", locations_[index].first);
		settings_->setValue("path", locations_[index].second);
	}
	settings_->endArray();
	settings_->sync();
	if (settings_->status() != QSettings::NoError) {
		notice_ = tr("Could not save dock preferences. Changes may be lost when OBS closes.");
		return false;
	}
	return true;
}

void FolderDock::updateLocations()
{
	const auto selected = presets_->currentData().toString();
	presets_->clear();
	for (const auto &location : locations_) {
		presets_->addItem(location.first, location.second);
		presets_->setItemData(presets_->count() - 1, location.second, Qt::ToolTipRole);
	}
	if (locations_.isEmpty())
		presets_->addItem(tr("No presets — select Edit presets"));
	else if (presets_->findData(selected) >= 0)
		presets_->setCurrentIndex(presets_->findData(selected));
	recents_->clear();
	for (const auto &path : recentPaths_)
		recents_->addItem(QDir::toNativeSeparators(path), path);
	if (recentPaths_.isEmpty())
		recents_->addItem(tr("No recent folders"));
	for (auto *combo : {presets_, recents_}) {
		combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		combo->setMinimumContentsLength(18);
	}
}

void FolderDock::rememberFolder(const QString &folder)
{
	const QString normalized = QDir::cleanPath(QDir::fromNativeSeparators(folder));
#ifdef Q_OS_WIN
	constexpr auto sensitivity = Qt::CaseInsensitive;
#else
	constexpr auto sensitivity = Qt::CaseSensitive;
#endif
	for (int i = static_cast<int>(recentPaths_.size()) - 1; i >= 0; --i)
		if (recentPaths_[i].compare(normalized, sensitivity) == 0)
			recentPaths_.removeAt(i);
	recentPaths_.prepend(normalized);
	while (recentPaths_.size() > 10)
		recentPaths_.removeLast();
	saveLocations();
	updateLocations();
}

void FolderDock::editPresets()
{
	if (editor_ || picker_ || waitingForStop_)
		return;
	auto *dialog = new QDialog(this);
	editor_ = dialog;
	dialog->setObjectName("presetEditor");
	dialog->setWindowTitle(tr("Recording folder presets"));
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->resize(650, 350);
	auto *layout = new QVBoxLayout(dialog);
	layout->addWidget(new QLabel(tr("Enter a name and full folder path. Presets can refer to drives currently disconnected."), dialog));
	auto *table = new QTableWidget(static_cast<int>(locations_.size()), 2, dialog);
	table->setObjectName("presetTable");
	table->setHorizontalHeaderLabels({tr("Name"), tr("Folder path")});
	table->horizontalHeader()->setStretchLastSection(true);
	table->setSelectionBehavior(QAbstractItemView::SelectRows);
	for (int row = 0; row < locations_.size(); ++row) {
		table->setItem(row, 0, new QTableWidgetItem(locations_[row].first));
		table->setItem(row, 1, new QTableWidgetItem(locations_[row].second));
	}
	layout->addWidget(table);
	auto *add = new QPushButton(tr("Add preset"), dialog);
	add->setObjectName("addPreset");
	auto *remove = new QPushButton(tr("Remove selected"), dialog);
	layout->addWidget(add);
	layout->addWidget(remove);
	connect(add, &QPushButton::clicked, dialog, [table] {
		if (table->rowCount() >= 50)
			return;
		const int row = table->rowCount();
		table->insertRow(row);
		table->setItem(row, 0, new QTableWidgetItem);
		table->setItem(row, 1, new QTableWidgetItem);
		table->setCurrentCell(row, 0);
		table->editItem(table->item(row, 0));
	});
	connect(remove, &QPushButton::clicked, dialog, [table] {
		for (int row = table->rowCount() - 1; row >= 0; --row)
			if (table->selectionModel()->isRowSelected(row, {}))
				table->removeRow(row);
	});
	auto *error = new QLabel(dialog);
	error->setWordWrap(true);
	layout->addWidget(error);
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, dialog);
	layout->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
	connect(buttons, &QDialogButtonBox::accepted, dialog, [this, dialog, table, error] {
		QList<QPair<QString, QString>> entries;
		QStringList names;
		for (int row = 0; row < table->rowCount(); ++row) {
			const QString name = table->item(row, 0) ? table->item(row, 0)->text().trimmed() : QString();
			const QString path = table->item(row, 1) ? table->item(row, 1)->text() : QString();
			if (name.isEmpty() || path.isEmpty() || !QFileInfo(path).isAbsolute() || names.contains(name, Qt::CaseInsensitive)) {
				error->setText(tr("Each preset needs a unique name and an absolute folder path."));
				return;
			}
			names.append(name);
			entries.append({name, QDir::toNativeSeparators(QDir::cleanPath(path))});
		}
		locations_ = entries;
		const bool saved = saveLocations();
		updateLocations();
		if (!saved) {
			error->setText(notice_);
			return;
		}
		dialog->accept();
	});
	connect(dialog, &QObject::destroyed, this, [this] { refresh(); });
	dialog->open();
	refresh();
}

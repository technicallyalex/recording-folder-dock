// SPDX-License-Identifier: GPL-2.0-or-later
#include "folder-dock.hpp"
#include <QMenu>
#include <QPersistentModelIndex>
#include <QTimer>
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

void FolderDock::updatePresetMenu()
{
	presetMenu_->clear();
	for (const auto &location : locations_) {
		const QString path = location.second;
		auto *action = presetMenu_->addAction(QString(location.first).replace("&", "&&"));
		action->setData(path);
		action->setToolTip(QDir::toNativeSeparators(path));
		action->setEnabled(unavailableReason(automatic_->isChecked()).isEmpty());
		connect(action, &QAction::triggered, this, [this, path] { chooseFolder(path); });
	}
	if (locations_.isEmpty())
		presetMenu_->addAction(tr("No presets — use + to add one"))->setEnabled(false);
}

void FolderDock::updateLocations()
{
	recentMenu_->clear();
	for (const auto &path : recentPaths_) {
		auto *action = recentMenu_->addAction(QDir::toNativeSeparators(path).replace("&", "&&"));
		action->setData(path);
		action->setEnabled(unavailableReason(automatic_->isChecked()).isEmpty());
		connect(action, &QAction::triggered, this, [this, path] { chooseFolder(path); });
	}
	if (recentPaths_.isEmpty())
		recentMenu_->addAction(tr("No recent folders"))->setEnabled(false);
	recentMenu_->addSeparator();
	auto *clear = recentMenu_->addAction(tr("Clear recents"));
	clear->setEnabled(!recentPaths_.isEmpty());
	connect(clear, &QAction::triggered, this, [this] {
		recentPaths_.clear();
		saveLocations();
		refresh();
	});
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
	table->setSelectionMode(QAbstractItemView::SingleSelection);
	for (int row = 0; row < locations_.size(); ++row) {
		table->setItem(row, 0, new QTableWidgetItem(locations_[row].first));
		table->setItem(row, 1, new QTableWidgetItem(locations_[row].second));
	}
	layout->addWidget(table);
	auto *add = new QPushButton(tr("Add preset"), dialog);
	add->setObjectName("addPreset");
	auto *remove = new QPushButton(tr("Remove selected"), dialog);
	auto *browse = new QPushButton(tr("Browse..."), dialog);
	browse->setObjectName("browsePreset");
	auto *tools = new QHBoxLayout;
	tools->addWidget(add);
	tools->addWidget(remove);
	tools->addWidget(browse);
	tools->addStretch();
	layout->addLayout(tools);
	connect(browse, &QPushButton::clicked, dialog, [dialog, table] {
		const int row = table->currentRow();
		if (row < 0) return;
		const QPersistentModelIndex cell(table->model()->index(row, 1));
		auto *picker = new QFileDialog(dialog, tr("Choose preset folder"), cell.data().toString());
		picker->setObjectName("presetFolderPicker");
		picker->setAttribute(Qt::WA_DeleteOnClose);
		picker->setFileMode(QFileDialog::Directory);
		picker->setOption(QFileDialog::ShowDirsOnly);
		connect(picker, &QDialog::accepted, dialog, [picker, table, cell] {
			if (cell.isValid() && !picker->selectedFiles().isEmpty())
				table->model()->setData(cell, QDir::toNativeSeparators(picker->selectedFiles().first()));
		});
		picker->open();
	});
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
	auto *use = buttons->addButton(tr("Use selected"), QDialogButtonBox::ActionRole);
	use->setObjectName("usePreset");
	auto selectionChanged = [table, browse, remove, use] {
		const bool selected = table->currentRow() >= 0;
		browse->setEnabled(selected);
		remove->setEnabled(selected);
		use->setEnabled(selected);
	};
	connect(table, &QTableWidget::itemSelectionChanged, dialog, selectionChanged);
	if (table->rowCount()) table->setCurrentCell(0, 0);
	selectionChanged();
	layout->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
	const auto save = [this, dialog, table, error](bool apply) {
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

		if (!saved) {
			error->setText(notice_);
			return;
		}
		const QString selected = apply && table->currentRow() >= 0 ? entries[table->currentRow()].second : QString();
		const auto revision = profileRevision_;
		dialog->accept();
		if (!selected.isEmpty())
			QTimer::singleShot(0, this, [this, selected, revision] {
				if (revision == profileRevision_ && !shuttingDown_) chooseFolder(selected);
			});
	};
	connect(buttons, &QDialogButtonBox::accepted, dialog, [save] { save(false); });
	connect(use, &QPushButton::clicked, dialog, [save] { save(true); });
	connect(dialog, &QObject::destroyed, this, [this] { refresh(); });
	dialog->open();
	refresh();
}

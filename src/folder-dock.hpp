// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <obs-frontend-api.h>
#include <QPointer>
#include <QWidget>
#include <QStringList>
#include <QSettings>
#include <memory>

class QMenu;
class QDialog;

class QFileDialog;
class QFileSystemWatcher;
class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;

class FolderDock final : public QWidget {
public:
	explicit FolderDock(QWidget *parent = nullptr, const QString &settingsFile = {});
	~FolderDock() override;
	bool applyFolder(const QString &folder, QString &error);
	void refresh();

private:
	static void onEvent(obs_frontend_event event, void *data);
	void chooseFolder(const QString &folder = {}, bool createMissing = false);
	void selectDestination();
	void finishSelection(const QString &folder);
	void editPresets();
	void loadLocations();
	bool saveLocations();
	void updateLocations();
	void updatePresetMenu();
	void rememberFolder(const QString &folder);
	void maintainPresetFolder(const QString &folder);
	void openPicker();
	void continueFolderChange();
	QString unavailableReason(bool ignoreOutputs = false) const;
	QLineEdit *path_;
	QFileSystemWatcher *folderWatcher_;
	QString recoveryError_;
	QLabel *folderAccess_;
	QPushButton *choose_;
	QPushButton *open_;
	QPushButton *automatic_;
	QPushButton *editPresets_;
	QPushButton *presets_;
	QMenu *presetMenu_;
	QPushButton *recents_;
	QMenu *recentMenu_;
	QPointer<QDialog> editor_;
	std::unique_ptr<QSettings> settings_;
	QList<QPair<QString, QString>> locations_;
	QStringList recentPaths_;
	QString pendingFolder_;
	QTimer *stopTimeout_;
	bool waitingForStop_ = false;
	bool automaticChange_ = false;
	bool recordingStopping_ = false;
	bool replayStopping_ = false;
	bool stopRecordingRequested_ = false;
	bool stopReplayRequested_ = false;
	QString notice_;
	QPointer<QFileDialog> picker_;
	bool recordingBusy_ = false;
	bool replayBusy_ = false;
	bool profileChanging_ = false;
	bool shuttingDown_ = false;
	unsigned long long profileRevision_ = 0;
};

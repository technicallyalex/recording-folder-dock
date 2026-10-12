// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <obs-frontend-api.h>
#include <QPointer>
#include <QWidget>
#include <QStringList>
#include <QSettings>
#include <memory>

class QComboBox;
class QDialog;

class QFileDialog;
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
	void chooseFolder(const QString &folder = {});
	void selectDestination();
	void finishSelection(const QString &folder);
	void editPresets();
	void loadLocations();
	bool saveLocations();
	void updateLocations();
	void rememberFolder(const QString &folder);
	void openPicker();
	void continueFolderChange();
	QString unavailableReason(bool ignoreOutputs = false) const;
	QLineEdit *path_;
	QLabel *status_;
	QPushButton *choose_;
	QPushButton *open_;
	QPushButton *automatic_;
	QComboBox *presets_;
	QComboBox *recents_;
	QPushButton *usePreset_;
	QPushButton *useRecent_;
	QPushButton *editPresets_;
	QPushButton *clearRecent_;
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

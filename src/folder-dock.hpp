// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <obs-frontend-api.h>
#include <QPointer>
#include <QWidget>
#include <QStringList>
#include <QSettings>
#include <QElapsedTimer>
#include <memory>
#include <functional>
#include <QHash>
#include <QThreadPool>
#include "folder-health.hpp"

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
	explicit FolderDock(QWidget *parent = nullptr, const QString &settingsFile = {},
		std::function<qint64()> clock = {}, bool synchronousChecks = false);
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
	bool saveLocations(const QList<QPair<QString, QString>> *candidate = nullptr,
		const QHash<QString, FolderVolume> *bindings = nullptr);
	void updateLocations();
	void updatePresetMenu();
	void rememberFolder(const QString &folder);
	void maintainPresetFolder(const QString &folder);
	void acceptFolderHealth(const FolderHealth &health, unsigned long long revision);
	qint64 now() const;
	enum class Operation { Idle, Stopping, Choosing, Restarting };
	Operation operation_ = Operation::Idle;
	std::function<qint64()> clock_;
	QElapsedTimer lifetime_;
	bool synchronousChecks_;
	QThreadPool healthPool_;
	bool healthPending_ = false;
	std::shared_ptr<std::atomic_bool> healthCancel_;
	QString healthPath_;
	qint64 nextHealthCheck_ = 0;
	int healthFailures_ = 0;
	FolderHealth health_;
	QHash<QString, FolderVolume> volumes_;
	void openPicker();
	void continueFolderChange();
	void continueReplayRestart();
	void beginReplayRestart(bool defer = false);
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
	qint64 stopStartedAt_ = 0;
	QTimer *changePoll_;
	qint64 restartStartedAt_ = 0;
	int restartAttempts_ = 0;
	unsigned long long restartRevision_ = 0;
	QString restartFolder_;
	QString restartKey_;
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

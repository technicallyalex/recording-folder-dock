// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <obs-frontend-api.h>
#include <QPointer>
#include <QWidget>

class QFileDialog;
class QLabel;
class QLineEdit;
class QPushButton;

class FolderDock final : public QWidget {
public:
	explicit FolderDock(QWidget *parent = nullptr);
	~FolderDock() override;
	bool applyFolder(const QString &folder, QString &error);
	void refresh();

private:
	static void onEvent(obs_frontend_event event, void *data);
	void chooseFolder();
	QString unavailableReason() const;
	QLineEdit *path_;
	QLabel *status_;
	QPushButton *choose_;
	QPushButton *open_;
	QPointer<QFileDialog> picker_;
	bool recordingBusy_ = false;
	bool replayBusy_ = false;
	bool profileChanging_ = false;
	bool shuttingDown_ = false;
	unsigned long long profileRevision_ = 0;
};

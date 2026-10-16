// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QString>
#include <QByteArray>
#include <atomic>
#include <memory>

struct FolderVolume {
	QByteArray device;
	QString root;
	bool operator==(const FolderVolume &other) const { return device == other.device && root == other.root; }
	bool known() const { return !device.isEmpty() && !root.isEmpty(); }
};

struct FolderHealth {
	QString path;
	bool accessible = false;
	bool exists = false;
	bool recovered = false;
	QString error;
	FolderVolume volume;
	QString parent;
	bool localWatch = false;
	bool canceled = false;
};

QString normalizedFolder(const QString &path);
FolderVolume folderVolume(const QString &path);
FolderHealth checkFolder(const QString &path, bool recover, const FolderVolume &expected,
	const std::shared_ptr<std::atomic_bool> &cancel = {});

// SPDX-License-Identifier: GPL-2.0-or-later
#include "folder-health.hpp"
#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

QString normalizedFolder(const QString &path)
{
	if (path.isEmpty()) return {};
	QString result = QDir::cleanPath(QDir::fromNativeSeparators(path));
#ifdef Q_OS_WIN
	result = result.toCaseFolded();
#endif
	return result;
}

FolderVolume folderVolume(const QString &path)
{
	// QStorageInfo needs an existing ancestor to resolve a missing destination.
	QString ancestor = path;
	while (!QFileInfo(ancestor).exists()) {
		const QString parent = QFileInfo(ancestor).absolutePath();
		if (parent == ancestor) return {};
		ancestor = parent;
	}
	const QStorageInfo storage(ancestor);
	if (!storage.isValid() || !storage.isReady()) return {};
	QByteArray device = storage.device();
#ifdef Q_OS_LINUX
	// Prefer a persistent filesystem UUID to /dev/sdX names, which can change on reconnect.
	const QString canonicalDevice = QFileInfo(QString::fromLocal8Bit(device)).canonicalFilePath();
	if (!canonicalDevice.isEmpty()) {
		for (const auto &entry : QDir("/dev/disk/by-uuid").entryInfoList(QDir::System | QDir::Files | QDir::NoDotAndDotDot))
			if (entry.canonicalFilePath() == canonicalDevice) { device = "uuid:" + entry.fileName().toUtf8(); break; }
	}
#endif
	return {device, normalizedFolder(storage.rootPath())};
}

FolderHealth checkFolder(const QString &path, bool recover, const FolderVolume &expected,
	const std::shared_ptr<std::atomic_bool> &cancel)
{
	FolderHealth result;
	result.path = path;
	if (cancel && cancel->load()) { result.canceled = true; return result; }
	if (path.isEmpty() || !QFileInfo(path).isAbsolute()) return result;
	result.volume = folderVolume(path);
	result.exists = QFileInfo(path).isDir();
	if (recover && (!result.volume.known() || (expected.known() && !(result.volume == expected)))) {
		result.error = QStringLiteral("The preset's original drive is unavailable. Folder recovery is paused.");
		return result;
	}
	if (recover && !result.exists) {
		if (cancel && cancel->load()) { result.canceled = true; return result; }
		// A legacy preset must first be observed or explicitly selected on its drive.
		if (!expected.known()) {
			result.error = QStringLiteral("Select this preset once to confirm its drive before automatic recovery.");
			return result;
		}
		if (!QDir().mkpath(path)) {
			result.error = QStringLiteral("Could not recreate preset folder. Check drive availability and permissions.");
			return result;
		}
		result.exists = result.recovered = true;
	}
	const QFileInfo info(path);
	result.accessible = result.exists && info.isReadable() && info.isWritable();
	if (!result.accessible)
		result.error = QStringLiteral("Recording folder is unavailable or read/write access is restricted.");
	result.parent = QFileInfo(path).absolutePath();
	const QStorageInfo storage(path);
	const auto type = storage.fileSystemType().toLower();
	result.localWatch = type != "cifs" && type != "smbfs" && type != "nfs" && type != "nfs4" &&
		!QDir::fromNativeSeparators(path).startsWith("//");
#ifdef Q_OS_WIN
	const QString root = QDir::toNativeSeparators(storage.rootPath());
	if (GetDriveTypeW(reinterpret_cast<LPCWSTR>(root.utf16())) == DRIVE_REMOTE) result.localWatch = false;
#endif
	return result;
}

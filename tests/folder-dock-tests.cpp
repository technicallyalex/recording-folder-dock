// SPDX-License-Identifier: GPL-2.0-or-later
#include "folder-dock.hpp"
#include <util/config-file.h>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QPushButton>
#include <QTimer>
#include <QLineEdit>
#include <QTemporaryDir>
#include <cstdio>
#include <cstdlib>

namespace {
config_t *profile = nullptr;
bool recording = false;
bool replay = false;
obs_frontend_event_cb callback = nullptr;
void *callbackData = nullptr;
int checks = 0;
int stopRecordingCalls = 0;
int stopReplayCalls = 0;
int startReplayCalls = 0;
void check(bool condition, const char *message)
{
	++checks;
	if (!condition) {
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}
QString get(const char *section, const char *key)
{
	const char *result = config_get_string(profile, section, key);
	return QString::fromUtf8(result ? result : "");
}
void event(obs_frontend_event type)
{
	check(callback != nullptr, "event callback registered");
	callback(type, callbackData);
}
}

extern "C" {
config_t *obs_frontend_get_profile_config(void) { return profile; }
bool obs_frontend_recording_active(void) { return recording; }
bool obs_frontend_replay_buffer_active(void) { return replay; }
void obs_frontend_recording_stop(void) { ++stopRecordingCalls; }
void obs_frontend_replay_buffer_stop(void) { ++stopReplayCalls; }
void obs_frontend_replay_buffer_start(void) { ++startReplayCalls; }
void obs_frontend_add_event_callback(obs_frontend_event_cb cb, void *data)
{
	callback = cb;
	callbackData = data;
}
void obs_frontend_remove_event_callback(obs_frontend_event_cb cb, void *data)
{
	check(cb == callback && data == callbackData, "removes its own event callback");
	callback = nullptr;
	callbackData = nullptr;
}
}

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	QTemporaryDir temp;
	check(temp.isValid(), "temporary directory");
	const QString ini = temp.filePath("profile.ini");
	check(config_open(&profile, ini.toUtf8().constData(), CONFIG_OPEN_ALWAYS) == CONFIG_SUCCESS, "open profile");
	const QString folder = temp.filePath(QString::fromUtf8("Recordings with spaces — 日本語"));
	check(QDir().mkpath(folder), "create Unicode folder");
	const QString expected = QDir::toNativeSeparators(folder);
	QString error;
	{
		FolderDock dock;
		config_set_string(profile, "AdvOut", "RecFilePath", "unchanged advanced");
		check(dock.applyFolder(folder, error), "simple path saved");
		check(get("SimpleOutput", "FilePath") == expected, "Unicode path preserved");
		check(get("AdvOut", "RecFilePath") == "unchanged advanced", "inactive mode unchanged");
		check(dock.findChild<QLineEdit *>("recordingFolderPath")->text() == expected, "dock displays saved path");
		config_t *saved = nullptr;
		check(config_open(&saved, ini.toUtf8().constData(), CONFIG_OPEN_EXISTING) == CONFIG_SUCCESS, "reopen profile");
		check(QString::fromUtf8(config_get_string(saved, "SimpleOutput", "FilePath")) == expected, "persisted on disk");
		config_close(saved);
		check(QDir(folder).entryList(QDir::Files | QDir::Hidden).isEmpty(), "write probe removed");
		check(!dock.applyFolder("", error) && !error.isEmpty(), "empty path rejected");
		check(!dock.applyFolder("relative", error), "relative path rejected");
		check(!dock.applyFolder(temp.filePath("missing"), error), "missing folder rejected");
		check(!dock.applyFolder(ini, error), "file path rejected");
		check(get("SimpleOutput", "FilePath") == expected, "invalid path leaves settings intact");
		recording = true;
		check(!dock.applyFolder(temp.path(), error), "active recording blocks change");
		recording = false;
		event(OBS_FRONTEND_EVENT_RECORDING_STARTING);
		check(!dock.applyFolder(temp.path(), error), "starting recording blocks change");
		event(OBS_FRONTEND_EVENT_RECORDING_STOPPING);
		check(!dock.applyFolder(temp.path(), error), "stopping recording blocks change");
		event(OBS_FRONTEND_EVENT_RECORDING_STOPPED);
		replay = true;
		check(!dock.applyFolder(temp.path(), error), "active replay blocks change");
		replay = false;
		event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING);
		check(!dock.applyFolder(temp.path(), error), "starting replay blocks change");
		event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED);
		config_set_string(profile, "Output", "Mode", "Advanced");
		check(dock.applyFolder(folder, error), "advanced standard saved");
		check(get("AdvOut", "RecFilePath") == expected, "advanced standard key");
		config_set_string(profile, "AdvOut", "RecType", "FFmpeg");
		config_set_bool(profile, "AdvOut", "FFOutputToFile", false);
		config_set_string(profile, "AdvOut", "FFURL", "rtmp://example.invalid/test");
		check(!dock.applyFolder(folder, error), "FFmpeg URL mode rejected");
		check(get("AdvOut", "FFURL") == "rtmp://example.invalid/test", "FFmpeg URL untouched");
		config_set_bool(profile, "AdvOut", "FFOutputToFile", true);
		check(dock.applyFolder(folder, error), "FFmpeg file mode saved");
		check(get("AdvOut", "FFFilePath") == expected, "FFmpeg file key");
		config_set_string(profile, "Output", "Mode", "Simple");
		auto *automatic = dock.findChild<QPushButton *>("automaticFolderChange");
		auto *choose = dock.findChild<QPushButton *>("chooseFolder");
		check(automatic && !automatic->isChecked(), "automatic mode defaults off");
		recording = replay = true;
		dock.refresh();
		check(!choose->isEnabled(), "manual mode remains locked while active");
		automatic->setChecked(true);
		check(choose->isEnabled(), "automatic mode permits choosing while active");
		choose->click();
		check(stopRecordingCalls == 1 && stopReplayCalls == 1, "both outputs asked to stop");
		check(!choose->isEnabled() && !automatic->isEnabled(), "operation cannot be duplicated or toggled halfway");
		QApplication::processEvents();
		check(!dock.findChild<QFileDialog *>(), "picker waits for stops");
		recording = false;
		event(OBS_FRONTEND_EVENT_RECORDING_STOPPED);
		QApplication::processEvents();
		check(!dock.findChild<QFileDialog *>(), "recording stop alone does not open picker");
		replay = false;
		event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED);
		QApplication::processEvents();
		auto *picker = dock.findChild<QFileDialog *>();
		check(picker != nullptr, "picker opens after both stop");
		picker->findChild<QLineEdit *>("fileNameEdit")->setText(temp.path());
		check(picker->selectedFiles().value(0) == temp.path(), "picker contains chosen directory");
		QMetaObject::invokeMethod(picker, "done", Qt::DirectConnection, Q_ARG(int, QDialog::Accepted));
		check(get("SimpleOutput", "FilePath") == QDir::toNativeSeparators(temp.path()), "picker saves selected path");
		check(startReplayCalls == 1, "replay requested only after successful selection");
		QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		dock.refresh();
		choose->click();
		QApplication::processEvents();
		picker = dock.findChild<QFileDialog *>();
		check(picker != nullptr, "idle automatic mode opens picker without stop requests");
		check(stopRecordingCalls == 1 && stopReplayCalls == 1, "idle outputs are not stopped");
		picker->reject();
		QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		check(startReplayCalls == 1, "cancel does not start replay");
		dock.refresh();
		replay = true;
		choose->click();
		event(OBS_FRONTEND_EVENT_PROFILE_CHANGING);
		event(OBS_FRONTEND_EVENT_PROFILE_CHANGED);
		replay = false;
		event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED);
		QApplication::processEvents();
		check(!dock.findChild<QFileDialog *>(), "profile change cancels pending picker");
		check(startReplayCalls == 1, "profile change does not restart replay");
		automatic->setChecked(false);
		event(OBS_FRONTEND_EVENT_PROFILE_CHANGING);
		check(!dock.applyFolder(temp.path(), error), "profile transition blocks changes");
		config_close(profile);
		check(config_open(&profile, temp.filePath("second.ini").toUtf8().constData(), CONFIG_OPEN_ALWAYS) == CONFIG_SUCCESS,
		      "second profile");
		config_set_string(profile, "SimpleOutput", "FilePath", temp.path().toUtf8().constData());
		event(OBS_FRONTEND_EVENT_PROFILE_CHANGED);
		check(dock.findChild<QLineEdit *>("recordingFolderPath")->text() == QDir::toNativeSeparators(temp.path()),
		      "switch profile refreshes dock");
		config_close(profile);
		profile = nullptr;
		check(!dock.applyFolder(folder, error), "missing profile rejected");
		// Remove our temporary profile's empty parent to make saving fail deterministically.
		const QString removedParent = temp.filePath("removed-parent");
		const QString unsavableIni = QDir(removedParent).filePath("profile.ini");
		check(QDir().mkpath(removedParent), "create temporary profile parent");
		profile = config_create(unsavableIni.toUtf8().constData());
		check(profile != nullptr, "create profile for save failure");
		check(QFile::remove(unsavableIni) && QDir().rmdir(removedParent), "remove temporary profile parent");
		config_set_string(profile, "SimpleOutput", "FilePath", "original");
		check(!dock.applyFolder(folder, error), "save failure reported");
		check(get("SimpleOutput", "FilePath") == "original", "save failure restores previous value");
		config_set_default_string(profile, "SimpleOutput", "FilePath", "default");
		config_remove_value(profile, "SimpleOutput", "FilePath");
		check(!dock.applyFolder(folder, error), "default save failure reported");
		check(!config_has_user_value(profile, "SimpleOutput", "FilePath"), "rollback preserves default inheritance");
		check(get("SimpleOutput", "FilePath") == "default", "rollback restores default path");
		event(OBS_FRONTEND_EVENT_EXIT);
		check(!dock.applyFolder(folder, error), "shutdown blocks writes");
	}
	check(callback == nullptr, "callback removed on destruction");
	config_close(profile);
	std::printf("PASS: %d checks\n", checks);
	return 0;
}

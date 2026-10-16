// SPDX-License-Identifier: GPL-2.0-or-later
#include "folder-dock.hpp"
#include <util/config-file.h>
#include <QApplication>
#include <QMenu>
#include <QDialogButtonBox>
#include <QTableWidget>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QPushButton>
#include <QTimer>
#include <QEventLoop>
#include <QLineEdit>
#include <QLabel>
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
	// Headless tests need Qt widgets; production uses the native platform picker.
	QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
	QApplication app(argc, argv);
	QTemporaryDir temp;
	check(temp.isValid(), "temporary directory");
	const QString ini = temp.filePath("profile.ini");
	check(config_open(&profile, ini.toUtf8().constData(), CONFIG_OPEN_ALWAYS) == CONFIG_SUCCESS, "open profile");
	const QString folder = temp.filePath(QString::fromUtf8("Recordings with spaces â€” æ—¥æœ¬èªž"));
	check(QDir().mkpath(folder), "create Unicode folder");
	const QString expected = QDir::toNativeSeparators(folder);
	QString error;
	{
		FolderDock dock(nullptr, {}, {}, true);
		config_set_string(profile, "AdvOut", "RecFilePath", "unchanged advanced");
		check(dock.applyFolder(folder, error), "simple path saved");
		check(get("SimpleOutput", "FilePath") == expected, "Unicode path preserved");
		check(get("AdvOut", "RecFilePath") == "unchanged advanced", "inactive mode unchanged");
		check(dock.findChild<QLineEdit *>("recordingFolderPath")->text() == expected, "dock displays saved path");
		auto *access = dock.findChild<QLabel *>("folderAccess");
		check(access && access->text() == QString(QChar(0x2713)), "available folder shows checkmark");
		config_set_string(profile, "SimpleOutput", "FilePath", temp.filePath("offline").toUtf8().constData());
		dock.refresh();
		check(access->text() == QString(QChar(0x00D7)), "missing folder shows cross");
		check(QDir().mkpath(temp.filePath("offline")), "restore folder");
		dock.refresh();
		check(access->text() == QString(QChar(0x2713)), "availability recovers without changing path");
		config_set_string(profile, "SimpleOutput", "FilePath", expected.toUtf8().constData());
		dock.refresh();
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
		event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED);
		event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED);
		QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		dock.refresh();
		choose->click();
		QApplication::processEvents();
		picker = dock.findChild<QFileDialog *>();
		check(picker != nullptr, "idle automatic mode opens picker without stop requests");
		check(stopRecordingCalls == 1 && stopReplayCalls == 1, "idle outputs are not stopped");
		picker->reject();
		QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		QApplication::processEvents();
		check(startReplayCalls == 2, "cancel restarts replay in automatic mode");
		check(get("SimpleOutput", "FilePath") == QDir::toNativeSeparators(temp.path()), "cancel preserves recording folder");
		event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED);
		event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED);
		dock.refresh();
		replay = true;
		choose->click();
		event(OBS_FRONTEND_EVENT_PROFILE_CHANGING);
		event(OBS_FRONTEND_EVENT_PROFILE_CHANGED);
		replay = false;
		event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED);
		QApplication::processEvents();
		check(!dock.findChild<QFileDialog *>(), "profile change cancels pending picker");
		check(startReplayCalls == 2, "profile change does not restart replay");
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
	check(config_open(&profile, temp.filePath("preferences-profile.ini").toUtf8().constData(), CONFIG_OPEN_ALWAYS) == CONFIG_SUCCESS,
	      "open preference test profile");
	const QString preferences = temp.filePath("dock-settings.ini");
	{
		FolderDock dock(nullptr, preferences, {}, true);
		dock.findChild<QPushButton *>("automaticFolderChange")->setChecked(true);
		dock.findChild<QPushButton *>("editPresets")->click();
		auto *editor = dock.findChild<QDialog *>("presetEditor");
		check(editor != nullptr, "preset editor opens");
		editor->findChild<QPushButton *>("addPreset")->click();
		auto *table = editor->findChild<QTableWidget *>("presetTable");
		table->item(0, 0)->setText("Game capture");
		editor->findChild<QPushButton *>("browsePreset")->click();
		auto *browser = editor->findChild<QFileDialog *>("presetFolderPicker");
		check(browser != nullptr, "preset browse opens folder picker");
		check(!browser->testOption(QFileDialog::DontUseNativeDialog), "preset browse permits native dialog");
		browser->findChild<QLineEdit *>("fileNameEdit")->setText(folder);
		QMetaObject::invokeMethod(browser, "done", Qt::DirectConnection, Q_ARG(int, QDialog::Accepted));
		check(table->item(0, 1)->text() == QDir::toNativeSeparators(folder), "browse fills preset path");
		QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		editor->findChild<QPushButton *>("addPreset")->click();
		table->item(1, 0)->setText("New folder");
		table->item(1, 1)->setText(ini);
		editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
		check(editor->isVisible(), "file collision keeps editor open");
		const QString newPresetPath = temp.filePath("new-parent/new-recordings");
		table->item(1, 1)->setText(newPresetPath);
		editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
		check(QFileInfo(newPresetPath).isDir(), "saving preset creates missing parents and folder");
		QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		check(QSettings(preferences, QSettings::IniFormat).value("presets/1/name").toString() == "Game capture", "named preset saved");
		check(dock.applyFolder(folder, error), "history records valid folder");
		check(dock.applyFolder(folder, error), "reusing folder succeeds");
		check(QSettings(preferences, QSettings::IniFormat).value("recentPaths").toStringList().size() == 1, "history deduplicates paths");
		check(!dock.applyFolder(temp.filePath("absent"), error), "invalid history path rejected");
		check(QSettings(preferences, QSettings::IniFormat).value("recentPaths").toStringList().size() == 1, "failed selection excluded from history");
	}
	{
		const int startsBefore = startReplayCalls;
		FolderDock dock(nullptr, preferences, {}, true);
		check(dock.findChild<QPushButton *>("automaticFolderChange")->isChecked(), "automatic toggle survives restart");
		check(startReplayCalls == startsBefore, "loading enabled toggle does not start replay");
		check(QSettings(preferences, QSettings::IniFormat).value("presets/1/name").toString() == "Game capture", "presets survive restart");
		check(QSettings(preferences, QSettings::IniFormat).value("recentPaths").toStringList().value(0) == folder, "history survives restart");
		recording = replay = true;
		dock.refresh();
		auto *presets = dock.findChild<QPushButton *>("presets")->menu();
		QMetaObject::invokeMethod(presets, "aboutToShow");
		check(presets->actions().first()->text() == "Game capture", "preset menu shows saved name");
		check(QDir().rmdir(folder), "delete saved preset folder");
		presets->actions().first()->trigger();
		check(QFileInfo(folder).isDir(), "selecting preset recreates deleted folder");
		QApplication::processEvents();
		check(startReplayCalls == startsBefore, "preset waits for output stops");
		recording = replay = false;
		event(OBS_FRONTEND_EVENT_RECORDING_STOPPED);
		event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED);
		QApplication::processEvents();
		check(startReplayCalls == startsBefore + 1, "preset follows automatic restart workflow");
		event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED);
		event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED);
		check(!dock.findChild<QFileDialog *>(), "preset bypasses picker");
		check(QDir().rmdir(folder), "delete current preset between recordings");
		QEventLoop recoveryWait;
		QTimer recoveryPoll;
		QObject::connect(&recoveryPoll, &QTimer::timeout, &recoveryWait, [&] {
			if (QFileInfo(folder).isDir()) recoveryWait.quit();
		});
		recoveryPoll.start(10);
		QTimer::singleShot(2000, &recoveryWait, &QEventLoop::quit);
		recoveryWait.exec();
		recoveryPoll.stop();
		check(QFileInfo(folder).isDir(), "watcher restores active preset without selecting it");
		check(startReplayCalls == startsBefore + 1, "folder recovery does not start outputs");
		check(QDir().rmdir(folder), "delete active preset during replay use");
		replay = true;
		dock.refresh();
		check(QFileInfo(folder).isDir(), "periodic recovery works while replay buffer is active");
		replay = false;
		const QString ordinaryFolder = temp.filePath("not-a-preset");
		config_set_string(profile, "SimpleOutput", "FilePath", ordinaryFolder.toUtf8().constData());
		dock.refresh();
		check(!QFileInfo(ordinaryFolder).exists(), "automatic recovery only creates configured presets");
		config_set_string(profile, "SimpleOutput", "FilePath", folder.toUtf8().constData());
		dock.refresh();
		dock.findChild<QPushButton *>("automaticFolderChange")->setChecked(false);
		check(QDir().rmdir(folder), "remove preset folder for collision test");
		QFile collision(folder);
		check(collision.open(QIODevice::WriteOnly), "create file blocking preset directory");
		collision.close();
		const QString previousFolder = get("SimpleOutput", "FilePath");
		const int startsBeforeFailure = startReplayCalls;
		presets->actions().first()->trigger();
		check(get("SimpleOutput", "FilePath") == previousFolder && startReplayCalls == startsBeforeFailure,
		      "failed recreation leaves recording settings and outputs unchanged");
		check(QFile::remove(folder), "remove collision fixture");
		for (int i = 0; i < 12; ++i) {
			const QString path = temp.filePath(QString("history-%1").arg(i));
			check(QDir().mkpath(path) && dock.applyFolder(path, error), "history folder applied");
		}
		check(QSettings(preferences, QSettings::IniFormat).value("recentPaths").toStringList().size() == 10, "history is limited to ten entries");
		auto *recent = dock.findChild<QPushButton *>("recents")->menu();
		QMetaObject::invokeMethod(recent, "aboutToShow");
		check(recent->actions().size() == 12, "flyout contains ten folders, separator and clear action");
		const auto chosen = recent->actions()[3]->data().toString();
		recent->actions()[3]->trigger();
		check(get("SimpleOutput", "FilePath") == QDir::toNativeSeparators(chosen), "recent path can be reused");
		QMetaObject::invokeMethod(recent, "aboutToShow");
		check(recent->actions().first()->data().toString() == chosen, "reused path moves to top");
		check(recent->actions().last()->text() == "Clear recents", "clear action is at bottom");
		recent->actions().last()->trigger();
	}
	{
		FolderDock dock(nullptr, preferences, {}, true);
		check(!dock.findChild<QPushButton *>("automaticFolderChange")->isChecked(), "disabled toggle survives restart");
		check(QSettings(preferences, QSettings::IniFormat).value("recentPaths").toStringList().value(0).isEmpty(), "history clearing persists");
	}
	{
		qint64 testTime = 0;
		FolderDock dock(nullptr, preferences, [&] { return testTime; }, true);
		dock.findChild<QPushButton *>("automaticFolderChange")->setChecked(true);
		auto *presets = dock.findChild<QPushButton *>("presets")->menu();
		QMetaObject::invokeMethod(presets, "aboutToShow");
		auto wait = [&](int milliseconds) {
			for (int elapsed = 0; elapsed < milliseconds; elapsed += 100) {
				testTime += qMin(100, milliseconds - elapsed);
				QMetaObject::invokeMethod(dock.findChild<QTimer *>("operationPoll"), "timeout");
				QApplication::processEvents();
			}
		};
		const int before = startReplayCalls;
		replay = true;
		event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED);
		presets->actions().first()->trigger();
		// STOPPED notification arrives before the frontend active flag settles.
		event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED);
		QApplication::processEvents();
		check(startReplayCalls == before, "stale active flag delays restart safely");
		replay = false;
		wait(400);
		check(startReplayCalls == before + 1, "polling resumes after stop flag settles without another event");
		check(!dock.findChild<QPushButton *>("presets")->isEnabled(), "switching is locked until replay restart confirms");
		wait(1600);
		check(startReplayCalls == before + 2, "ignored replay start request is retried");
		event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING);
		wait(1700);
		check(startReplayCalls == before + 2, "accepted start is not duplicated while starting");
		replay = true;
		event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED);
		check(dock.findChild<QPushButton *>("presets")->isEnabled(), "confirmation unlocks preset controls");
		wait(1700);
		check(startReplayCalls == before + 2, "confirmed replay restart ends retries");
		replay = false;
		event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED);
		presets->actions().first()->trigger();
		const int after = startReplayCalls;
		event(OBS_FRONTEND_EVENT_PROFILE_CHANGING);
		event(OBS_FRONTEND_EVENT_PROFILE_CHANGED);
		wait(1700);
		check(startReplayCalls == after, "profile change cancels outstanding replay restart");
		presets->actions().first()->trigger();
		const int beforeStop = startReplayCalls;
		event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPING);
		event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED);
		wait(1700);
		check(startReplayCalls == beforeStop, "user stop cancels pending restart retries");
		const int beforeFailure = startReplayCalls;
		presets->actions().first()->trigger();
		wait(10500);
		check(startReplayCalls == beforeFailure + 3, "failed replay restart attempts are bounded");
		check(dock.findChild<QPushButton *>("presets")->isEnabled(), "restart timeout unlocks dock controls");
		check(dock.toolTip().contains("did not restart"), "restart failure is reported");
		presets->actions().first()->trigger();
		event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING);
		wait(10500);
		dock.findChild<QPushButton *>("automaticFolderChange")->setChecked(false);
		check(dock.applyFolder(temp.path(), error), "failed STARTING without STOPPED does not leave dock blocked");
	}
	{
		FolderDock dock(nullptr, preferences, {}, true);
		auto *automatic = dock.findChild<QPushButton *>("automaticFolderChange");
		auto *choose = dock.findChild<QPushButton *>("chooseFolder");
		automatic->setChecked(true);
		const QString original = get("SimpleOutput", "FilePath");
		const int before = startReplayCalls;
		const int recordStops = stopRecordingCalls;
		replay = true;
		event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED);
		choose->click();
		replay = false;
		event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED);
		QApplication::processEvents();
		auto *picker = dock.findChild<QFileDialog *>();
		check(picker != nullptr, "cancel test opens picker after stopping replay");
		picker->close();
		QApplication::processEvents();
		QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		check(startReplayCalls == before + 1, "closing folder dialog resumes stopped replay");
		check(get("SimpleOutput", "FilePath") == original, "closing dialog preserves original destination");
		check(stopRecordingCalls == recordStops, "cancel recovery does not affect recording");
		event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED);
		event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED);
		automatic->setChecked(false);
		choose->click();
		picker = dock.findChild<QFileDialog *>();
		check(picker != nullptr, "manual cancel opens picker");
		picker->reject();
		QApplication::processEvents();
		QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		check(startReplayCalls == before + 1, "manual-mode cancel does not start replay");
		automatic->setChecked(true);
		choose->click();
		event(OBS_FRONTEND_EVENT_PROFILE_CHANGING);
		event(OBS_FRONTEND_EVENT_PROFILE_CHANGED);
		QApplication::processEvents();
		QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		check(startReplayCalls == before + 1, "profile-triggered dialog rejection does not resume replay");
	}
	{
		FolderDock dock(nullptr, preferences, {}, true);
		dock.findChild<QPushButton *>("editPresets")->click();
		auto *editor = dock.findChild<QDialog *>("presetEditor");
		auto *table = editor->findChild<QTableWidget *>("presetTable");
		const QString original = table->item(0, 0)->text();
		table->item(0, 0)->setText("Unsaved edit");
		const QString backup = preferences + ".backup";
		check(QFile::rename(preferences, backup) && QDir().mkdir(preferences), "block settings destination");
		editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
		check(editor->isVisible(), "failed preferences save keeps editor open");
		editor->reject();
		QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		auto *menu = dock.findChild<QPushButton *>("presets")->menu();
		QMetaObject::invokeMethod(menu, "aboutToShow");
		check(menu->actions().first()->text() == original, "failed save does not change live presets");
		check(QDir().rmdir(preferences) && QFile::rename(backup, preferences), "restore settings destination");
		auto *automatic = dock.findChild<QPushButton *>("automaticFolderChange");
		automatic->setChecked(!automatic->isChecked());
		check(QSettings(preferences, QSettings::IniFormat).value("presets/1/name").toString() == original,
		      "later save does not leak canceled preset edits");
	}
	{
		const QString guarded = temp.filePath("guarded/preset");
		const auto volume = folderVolume(temp.path());
		check(volume.known(), "resolve temporary folder backing volume");
		auto wrongVolume = volume;
		wrongVolume.device += "-different-volume";
		const auto blocked = checkFolder(guarded, true, wrongVolume);
		check(!blocked.error.isEmpty() && !QFileInfo(guarded).exists(), "volume mismatch never creates fallback directories");
		const auto unbound = checkFolder(guarded, true, {});
		check(!unbound.error.isEmpty() && !QFileInfo(guarded).exists(), "unbound missing legacy preset is not recreated blindly");
		const auto recovered = checkFolder(guarded, true, volume);
		check(recovered.recovered && QFileInfo(guarded).isDir(), "matching backing volume allows recovery");
		const QString canceledPath = temp.filePath("canceled/preset");
		const auto cancel = std::make_shared<std::atomic_bool>(true);
		check(checkFolder(canceledPath, true, volume, cancel).canceled && !QFileInfo(canceledPath).exists(),
		      "obsolete background check does not create directories");
	}
	{
		config_set_string(profile, "SimpleOutput", "FilePath", temp.path().toUtf8().constData());
		FolderDock dock;
		QEventLoop loop;
		QTimer poll;
		QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
			if (dock.findChild<QLabel *>("folderAccess")->property("available").toBool()) loop.quit();
		});
		poll.start(10);
		QTimer::singleShot(3000, &loop, &QEventLoop::quit);
		loop.exec();
		check(dock.findChild<QLabel *>("folderAccess")->property("available").toBool(), "background health check updates dock asynchronously");
	}
	{
		const QString path = temp.filePath("async-rebind");
		check(QDir().mkpath(path), "create async rebind destination");
		const QString settingsPath = temp.filePath("async-rebind.ini");
		{
			QSettings settings(settingsPath, QSettings::IniFormat);
			settings.beginWriteArray("presets", 1);
			settings.setArrayIndex(0);
			settings.setValue("name", "Async rebind");
			settings.setValue("path", path);
			settings.setValue("device", QByteArray("wrong-drive").toBase64());
			settings.setValue("volumeRoot", folderVolume(path).root);
			settings.endArray();
			settings.sync();
		}
		config_set_string(profile, "SimpleOutput", "FilePath", path.toUtf8().constData());
		FolderDock dock(nullptr, settingsPath);
		check(dock.applyFolder(path, error), "explicit selection rebinds current preset drive");
		QEventLoop loop;
		QTimer poll;
		QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
			if (dock.findChild<QLabel *>("folderAccess")->property("available").toBool()) loop.quit();
		});
		poll.start(10);
		QTimer::singleShot(1000, &loop, &QEventLoop::quit);
		loop.exec();
		check(dock.findChild<QLabel *>("folderAccess")->property("available").toBool(),
		      "old background result cannot override updated preset drive binding");
		check(!dock.toolTip().contains("original drive is unavailable"), "obsolete drive warning is discarded");
	}
	config_close(profile);
	std::printf("PASS: %d checks\n", checks);
	return 0;
}

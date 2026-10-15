// SPDX-License-Identifier: GPL-2.0-or-later
// Capture the production widgets with isolated sample data, without changing OBS.
#include "folder-dock.hpp"
#include <util/config-file.h>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFont>
#include <QMenu>
#include <QLineEdit>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <cstdio>

namespace {
config_t *profile = nullptr;
}

extern "C" {
config_t *obs_frontend_get_profile_config(void) { return profile; }
bool obs_frontend_recording_active(void) { return false; }
bool obs_frontend_replay_buffer_active(void) { return false; }
void obs_frontend_recording_stop(void) {}
void obs_frontend_replay_buffer_stop(void) {}
void obs_frontend_replay_buffer_start(void) {}
void obs_frontend_add_event_callback(obs_frontend_event_cb, void *) {}
void obs_frontend_remove_event_callback(obs_frontend_event_cb, void *) {}
}

int main(int argc, char **argv)
{
  QApplication app(argc, argv);
  if (argc != 2) {
    std::fprintf(stderr, "Usage: folder-dock-screenshots OUTPUT_DIRECTORY\n");
    return 1;
  }
  app.setStyle("Fusion");
  app.setFont(QFont("Segoe UI", 10));
  QPalette palette;
  palette.setColor(QPalette::Window, QColor("#1d1f26"));
  palette.setColor(QPalette::WindowText, QColor("#e1e1e1"));
  palette.setColor(QPalette::Base, QColor("#272a33"));
  palette.setColor(QPalette::AlternateBase, QColor("#323540"));
  palette.setColor(QPalette::Text, QColor("#e1e1e1"));
  palette.setColor(QPalette::Button, QColor("#323540"));
  palette.setColor(QPalette::ButtonText, QColor("#e1e1e1"));
  palette.setColor(QPalette::Highlight, QColor("#284cb8"));
  palette.setColor(QPalette::HighlightedText, Qt::white);
  app.setPalette(palette);
  app.setStyleSheet("QPushButton { padding: 7px 12px; border: 1px solid #464b59; border-radius: 4px; }"
    "QPushButton:checked { background: #284cb8; border-color: #476bd7; }"
    "QLineEdit { padding: 7px; border: 1px solid #464b59; border-radius: 4px; }"
    "QMenu { padding: 6px; border: 1px solid #464b59; }"
    "QMenu::item { padding: 8px 20px; }"
    "QHeaderView::section { background: #323540; padding: 7px; border: 0; }"
    "QTableWidget { gridline-color: #464b59; }");
  QTemporaryDir temp;
  const QDir output(QString::fromLocal8Bit(argv[1]));
  if (!temp.isValid() || !QDir().mkpath(output.absolutePath())) return 1;
  if (config_open(&profile, temp.filePath("profile.ini").toUtf8().constData(), CONFIG_OPEN_ALWAYS) != CONFIG_SUCCESS)
    return 1;
  // Read/write state uses a temporary fixture; displayed sample paths are portable.
  const QString sampleRoot = "D:/Game Capture";
  const QString current = QDir(sampleRoot).filePath("Test 1");
  config_set_string(profile, "SimpleOutput", "FilePath", temp.path().toUtf8().constData());
  const QString settingsFile = temp.filePath("settings.ini");
  {
    QSettings settings(settingsFile, QSettings::IniFormat);
    settings.setValue("autoStop", true);
    settings.setValue("recentPaths", QStringList{current, QDir(sampleRoot).filePath("Test 2"), QDir(sampleRoot).filePath("Highlights")});
    settings.beginWriteArray("presets", 3);
    const QStringList names{"Game capture", "Highlights", "Tutorials"};
    for (int i = 0; i < names.size(); ++i) {
      settings.setArrayIndex(i);
      settings.setValue("name", names[i]);
      settings.setValue("path", i == 0 ? current : QDir(sampleRoot).filePath(names[i]));
    }
    settings.endArray();
    settings.sync();
  }
  bool ok = true;
  {
    FolderDock dock(nullptr, settingsFile, {}, true);
    dock.resize(520, 255);
    dock.show();
    QApplication::processEvents();
    dock.findChild<QLineEdit *>("recordingFolderPath")->setText(QDir::toNativeSeparators(current));
    auto capture = [&output, &ok](QWidget *widget, const QString &name) {
      QApplication::processEvents();
      ok = widget->grab().save(output.filePath(name + ".png")) && ok;
    };
    capture(&dock, "dock");
    auto *recent = dock.findChild<QPushButton *>("recents")->menu();
    QMetaObject::invokeMethod(recent, "aboutToShow");
    recent->ensurePolished();
    recent->resize(recent->sizeHint());
    capture(recent, "recents");
    auto *presets = dock.findChild<QPushButton *>("presets")->menu();
    QMetaObject::invokeMethod(presets, "aboutToShow");
    presets->ensurePolished();
    presets->resize(presets->sizeHint());
    capture(presets, "preset-menu");
    dock.findChild<QPushButton *>("editPresets")->click();
    auto *editor = dock.findChild<QDialog *>("presetEditor");
    if (!editor) return 1;
    editor->resize(900, 350);
    capture(editor, "presets");
    editor->reject();
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    config_set_string(profile, "SimpleOutput", "FilePath", temp.filePath("offline").toUtf8().constData());
    dock.refresh();
    dock.findChild<QLineEdit *>("recordingFolderPath")->setText(QDir::toNativeSeparators("E:/Game Capture/Offline drive"));
    capture(&dock, "unavailable");
  }
  config_close(profile);
  return ok ? 0 : 1;
}

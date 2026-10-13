# Development

Builds require CMake 3.28+, a C++17 compiler, OBS frontend development files, and Qt 6 Widgets. The Windows script downloads pinned OBS and Qt dependencies automatically.

## Windows

Install Visual Studio 2022 Build Tools with **Desktop development with C++**, a Windows SDK, and CMake tools.

```powershell
.\scripts\build-windows.ps1 -Test
```

## Linux

On Ubuntu 24.04 with the OBS Project PPA configured:

```sh
sudo apt install cmake ninja-build g++ pkg-config python3 obs-studio qt6-base-dev libsimde-dev
bash scripts/build-linux.sh
```

For other distributions, install their OBS and Qt development packages. The OBS PPA includes its own headers; do not mix it with Ubuntu's separate `libobs-dev` package.

## Flatpak

Install Flatpak, flatpak-builder, Python 3, and Git. Commit source changes before building; the script uses `git archive HEAD` and the stable OBS Flatpak's matching SDK.

```sh
bash scripts/build-flatpak.sh
```

Packages are written to `dist/`. Each version has its own output; scripts refuse to overwrite an existing package. Builds do not install the plugin into OBS.

## Validation

CI builds and tests Windows, Ubuntu 24.04 with the OBS PPA, and the OBS Flatpak runtime. Tests use real Qt widgets and OBS configuration I/O with controlled frontend callbacks. They cover output-mode selection, recording/replay sequencing, profile changes, failed-save recovery, preferences, preset folder creation, history, and folder availability.

Before releasing, check the plugin inside OBS: dock layout and restart, native folder selection/cancellation, actual recording and replay destinations, unavailable folders, and clean shutdown. Linux desktop checks include X11/Wayland and Flatpak folder permissions. Automated checks do not establish compatibility with every Linux distribution.

## Screenshots

Run `scripts/build-windows.ps1 -Test -Screenshots` to capture the real plugin widgets with sample data in a standalone Qt harness. The PNGs go into `media/` and do not modify OBS settings. The harness uses a dark demonstration style; OBS supplies its own theme in production.

## Release

Bump `buildspec.json`, run all three build jobs, and publish their separate packages with a SHA-256 checksum file. Keep older tags and release assets intact.

Based on the [OBS plugin template](https://github.com/obsproject/obs-plugintemplate). Licensed under [GPL-2.0-or-later](../LICENSE).

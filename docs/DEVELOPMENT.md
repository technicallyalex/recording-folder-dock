# Development

Builds require CMake 3.28+, a C++17 compiler, OBS frontend development files, and Qt 6 Widgets. The Windows script downloads pinned OBS and Qt dependencies automatically.

Release packages target the OBS 33 plugin loader. Windows keeps its older pinned SDK, and native Linux uses the OBS PPA's available development files; no new frontend API is required. The installation layout sets the minimum supported OBS version. OBS 31.1–32 users should keep v1.4.0.

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

## Reliability checks

Routine folder probes and recovery run in a private worker pool; filesystem notifications are coalesced while a check is running. Failed probes back off to at most one check every 32 seconds. Local watchers can trigger an earlier check; network paths use polling. Native filesystem calls cannot be forcibly canceled, so module unload waits for an outstanding probe to finish safely.

Preset volume identity is stored in the settings file. Linux uses a filesystem UUID when available, otherwise the device and mount root. Legacy presets acquire this binding when observed on an available folder. A missing unbound preset must be explicitly selected once; automatic recovery will not guess which drive it belongs to. Use Choose folder to explicitly rebind a moved destination.

Replay and stop deadlines use an injected monotonic clock in tests. The UI tests run synchronous folder probes except for a separate background-probe integration check. Timing tests advance the clock instead of sleeping through production timeouts. OBS's log records recovery failures, stop/restart timeouts, and restart attempts.

## Screenshots

Run `scripts/build-windows.ps1 -Test -Screenshots` to capture the real plugin widgets with sample data in a standalone Qt harness. The PNGs go into `media/` and do not modify OBS settings. The harness uses a dark demonstration style; OBS supplies its own theme in production.

## Release

Bump `buildspec.json` and merge the change, then push the matching tag (for example `v1.4.0`). The Release workflow validates the version, runs platform builds and release-tool tests, and publishes the three separate packages with SHA-256 checksums. Existing release assets are never replaced. If publication fails, a draft may remain for inspection.

Run `python -m unittest discover -s tests -p 'test_*.py'` to test release staging locally.

The packaged Windows installer test also covers migration backups, portable installs, repeat installs, and version guards in isolated folders. Linux installer fixtures run on Linux with temporary XDG paths and mocked OBS/dependency commands. Flatpak builds check the installed module and locale paths before bundling.

OBS 33 paths were checked against the [migration guide](https://obsproject.com/kb/legacy-plugin-developer-info) and the [platform loader implementations](https://github.com/obsproject/obs-studio/tree/33.0.0/frontend/plugin-manager). Native system installs use `lib/obs-modules/plugins` and `share/obs/obs-modules/plugins`; Flatpak extensions use `lib/obs-modules/plugins` and `share/obs-modules/plugins`. Native per-user installs use `$XDG_DATA_HOME/obs-studio/plugins/<name>/<name>.so` with adjacent `data`. Do not use the old general Plugins Guide's folder structure for these packages.

Based on the [OBS plugin template](https://github.com/obsproject/obs-plugintemplate). Licensed under [GPL-2.0-or-later](../LICENSE).

# Recording Folder Dock for OBS

A native dock with **Choose folder...** and **Open folder** buttons. Select a folder and OBS uses it for the next recording in the current profile. The dock follows OBS's theme and can be docked, floated, resized, or hidden.

## Install on Windows

Download the Windows ZIP from [GitHub Releases](https://github.com/technicallyalex/recording-folder-dock/releases/latest).

Requires 64-bit OBS Studio 31.1 or newer. The Windows release is built against OBS 31.1.1 and Qt 6.8.3; it uses the Qt libraries supplied by OBS.

1. Close OBS and extract `recording-folder-dock-windows-x64.zip`.
2. Copy the extracted `recording-folder-dock` folder into `C:\ProgramData\obs-studio\plugins\`. Accept Windows's administrator prompt if shown. The DLL should end up at `C:\ProgramData\obs-studio\plugins\recording-folder-dock\bin\64bit\recording-folder-dock.dll`.
3. Start OBS and enable **Docks > Recording Folder**.
4. Click **Choose folder...** and select a folder. The displayed path is the saved destination. Drag the dock into your preferred position.

Alternatively, run the included `install-windows.ps1` from an Administrator PowerShell after extracting the ZIP. For a portable OBS installation, copy the DLL into that installation's `obs-plugins\64bit` folder instead. Do not install it in both locations.

To uninstall, close OBS and remove only the `recording-folder-dock` plugin folder (or the DLL if installed in portable OBS). Your chosen recording destination remains saved in OBS.

## Behavior

- Saves immediately to the current profile's active output mode: Simple, Advanced Standard, or Advanced Custom FFmpeg with **Output to File** enabled.
- Changes only the active mode's recording folder. Switching to another output mode shows that mode's existing folder.
- Checks the folder exists and tests write access using a temporary file that is automatically removed. Unicode, spaces, and writable network folders are supported.
- Disables changes while recording (including paused recordings) or the replay buffer is starting, active, or stopping. Replay recordings share OBS's recording folder, so subsequent replay sessions also use the selected folder.
- Refreshes after profile changes and after Output Settings changes. Close OBS Settings before using the dock. A pending folder picker is canceled if the profile changes.
- Custom FFmpeg output to a URL has no recording folder; the dock explains how to enable file output.
- A failed profile save restores the previous in-memory setting and shows an error.

This does not move existing recordings, change filename formatting, start a recording, or modify encoder settings. OBS filename formatting may create additional subfolders beneath the selected destination.

## Build from source

Install Visual Studio 2022 Build Tools with **Desktop development with C++**, a Windows 10/11 SDK, and **C++ CMake tools for Windows** (CMake 3.28+).

```powershell
.\scripts\build-windows.ps1 -Test
```

The first build downloads official OBS sources and Qt/dependency archives specified and hash-pinned in `buildspec.json`. It builds, tests, and creates the installable ZIP in `dist`. No installation into OBS occurs during a build.

Manual build:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
cmake --install build_x64 --config RelWithDebInfo --prefix dist/package
```

The official template's macOS and Linux build support is retained, but only Windows is validated for this release. See [TESTING.md](TESTING.md) for test coverage and remaining interactive checks.

## Development references

- [OBS frontend dock and profile APIs](https://docs.obsproject.com/reference-frontend-api)
- [Official OBS plugin template](https://github.com/obsproject/obs-plugintemplate)
- [OBS recording output implementation](https://github.com/obsproject/obs-studio/blob/31.1.1/frontend/utility/AdvancedOutput.cpp)

GPL-2.0-or-later. See [LICENSE](LICENSE).

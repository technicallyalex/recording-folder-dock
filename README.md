# Recording Folder Dock

Change your OBS recording folder from a dock. Save presets, return to recent folders, and switch destinations without opening Output Settings.

[**Download**](https://github.com/technicallyalex/recording-folder-dock/releases/latest) · [Report an issue](https://github.com/technicallyalex/recording-folder-dock/issues) · [Build from source](docs/DEVELOPMENT.md)

![Recording Folder Dock](media/dock.png)

## Features

- **Choose folder** selects a destination; **Open folder** opens it in your file manager.
- **Presets** applies a saved location. **+** opens configuration with folder browsing and manual paths. Saving a new path creates missing folders. The active preset folder is automatically recreated if deleted, keeping it ready for recording and replay saves. Recovery pauses if its original drive is unavailable.
- **Recents** lists the last ten successful destinations, with **Clear recents** at the bottom.
- **Auto-stop / restart replay** stops recording and replay before a folder change, then starts replay after a successful selection. The toggle remembers its state.
- A green **✓** indicates the folder exists and reports read/write access; a red **×** indicates it is unavailable or restricted.

## Installation

Download the package for your OBS installation, close OBS, and follow the matching instructions:

| Platform | Package | Install |
| --- | --- | --- |
| Windows x64 | `windows-x64.zip` | Extract and run `install-windows.ps1` in an Administrator PowerShell. |
| Native Linux x86-64 | `linux-x86_64.tar.gz` | Extract, then run `bash install-linux.sh` from the extracted folder. |
| OBS Flatpak x86-64 | `flatpak-x86_64.flatpak` | Run `flatpak install --user ./recording-folder-dock-VERSION-flatpak-x86_64.flatpak`. |

Restart OBS and enable **Docks → Recording Folder**.

These packages require OBS 33 or newer. For OBS 31.1–32, use [v1.4.0](https://github.com/technicallyalex/recording-folder-dock/releases/tag/v1.4.0). The native Linux package targets Ubuntu 24.04 with the OBS Project PPA; other distributions can [build from source](docs/DEVELOPMENT.md) or use OBS Flatpak. Flatpak folder access follows OBS's sandbox permissions.

On Windows, the DLL now sits directly in `C:\ProgramData\obs-studio\plugins\recording-folder-dock\`, beside `data`. For a custom OBS location, run `.\install-windows.ps1 -ObsPath "D:\OBS"`; for portable OBS, add `-Portable` to install under `<OBS root>\plugins\`. The installer backs up previous plugin files under `obs-studio\plugin-backups` (portable: `<OBS root>\plugin-backups`) before removing this plugin's legacy copies. Presets and OBS settings are preserved.

Native Linux installs to `${XDG_DATA_HOME:-~/.local/share}/obs-studio/plugins/recording-folder-dock/`, with the `.so` beside `data`. Its installer backs up the old per-user plugin folder and removes its old module; distribution-managed system copies should be removed through their package manager. Flatpak packages use OBS 33's extension paths; update using `flatpak install --user` as above.

These paths follow [OBS's migration guide](https://obsproject.com/kb/legacy-plugin-developer-info) and the [OBS 33 Linux/Flatpak loaders](https://github.com/obsproject/obs-studio/tree/33.0.0/frontend/plugin-manager). To uninstall, close OBS and remove the installed plugin folder; Flatpak users can run `flatpak uninstall --user com.obsproject.Studio.Plugin.RecordingFolderDock`.

## Using the dock

Folder changes apply to the current profile's recording output, including subsequent replay sessions. Presets, recents, and the automatic toggle are shared across profiles. Existing recordings stay where they are.

With automatic mode off, stop recording and replay before changing folders. With it on, recording stays stopped after a change; only replay restarts. Canceling resumes replay on the unchanged folder; recording stays stopped. **Stopping replay discards unsaved replay footage.** Enable Replay Buffer in OBS Output Settings to use automatic restart.

The dock supports Simple, Advanced Standard, and Custom FFmpeg with **Output to File** enabled. Folder selection checks actual write access; the availability icon is a lightweight filesystem check.

Licensed under [GPL-2.0-or-later](LICENSE).

## AI disclosure

ChatGPT was used in development. The maintainer recorded this in [the original disclosure](https://github.com/technicallyalex/recording-folder-dock/commit/7da2a78a85f11a3479d6d6ccaa18795d8f10d582). That record does not identify models, affected files, specific tasks, or the proportion of AI-generated material, so those details remain unverified.

OpenAI Codex was used for the 2026-10-31 repository audit and to draft these disclosure and licensing documentation changes. This describes this audit only and does not establish earlier Codex use.

## Licensing and third-party material

The project is [GPL-2.0-or-later](LICENSE), as stated in its source headers. The [third-party and distribution notes](docs/THIRD_PARTY_NOTICES.md) explain the OBS template provenance, OBS/Qt dependencies, screenshots, source availability, and remaining provenance limits. AI assistance does not change these terms or establish ownership of third-party material.

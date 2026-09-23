# Validation

Run `scripts/build-windows.ps1 -Test` to compile the plugin and run the native test harness. The harness uses real Qt widgets and OBS's configuration/file-writing library, with controlled frontend profile and recording-state callbacks.

## Automated coverage

- Simple, Advanced Standard, and FFmpeg-to-file destination keys.
- Leaves inactive output modes and FFmpeg URLs unchanged.
- UTF-8/Unicode names and spaces; reads the saved profile back from disk.
- Temporary write probes are removed.
- Empty, relative, nonexistent, and non-directory paths are rejected.
- Active recording/replay and recording start/stop transitions block changes.
- Profile transitions, absent profiles, and shutdown block changes.
- Switching profiles refreshes the dock's displayed path.
- Failed saves restore both explicit values and default inheritance.
- Frontend callbacks are removed when the widget is destroyed.

The Windows DLL was also loaded against the locally installed OBS Studio 32.2.1 runtime (Qt 6.11.1), and its OBS module entry points were verified. Loading the DLL alone does not exercise docking or recording.

## Remaining interactive OBS checks

These require running the installed plugin inside OBS and have not been performed:

1. Enable **Docks > Recording Folder**, move/resize it, restart OBS, and confirm dock layout is restored.
2. Choose a writable folder, make a short recording, and verify the file lands there. Repeat in Simple, Advanced Standard, and FFmpeg-to-file modes.
3. Cancel the folder picker and confirm the destination is unchanged.
4. Start and pause recording; confirm **Choose folder...** is disabled. Repeat with replay buffer active.
5. Change profiles and change Output Settings; confirm the displayed destination updates.
6. Try a folder without write permission or an unavailable network share; confirm the destination is unchanged and an error appears.
7. Confirm **Open folder** opens the destination in the file manager.
8. Close OBS with the dock or picker open and confirm a clean shutdown.

macOS and Linux builds are not validated.

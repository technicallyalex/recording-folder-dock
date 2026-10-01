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

## v1.1.0 automatic-mode coverage

Tests exercise the real folder picker with controlled asynchronous output callbacks: default-off behavior, both stop requests, waiting for both stopped events, saving before replay restart, cancellation, idle outputs, and canceling pending work on profile changes.

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

9. Enable automatic mode while recording and replay are active. Choose a folder; verify both stop before the picker opens and only replay restarts after selecting a path. Confirm replay saves into the new folder. Cancel once and verify both remain stopped.

## v1.1.1 native picker

Production uses the platform-native folder picker. The headless test application explicitly opts out of native dialogs so it can exercise the selection callbacks without desktop interaction. Verify the native Windows folder picker appearance, selection, and cancellation in OBS; the headless suite does not validate Windows shell UI.

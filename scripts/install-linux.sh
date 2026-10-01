#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
[[ $(uname -s) == Linux ]] || { echo 'This installer is for Linux.' >&2; exit 1; }
if pgrep -x obs >/dev/null; then
    echo 'Close OBS before installing.' >&2
    exit 1
fi
plugin=recording-folder-dock/bin/64bit/recording-folder-dock.so
[[ -f "$plugin" ]] || { echo 'Extract the Linux package before running this installer.' >&2; exit 1; }
if ! dependencies=$(ldd "$plugin" 2>&1) || grep -q 'not found' <<< "$dependencies"; then
    printf '%s\n' "$dependencies" >&2
    echo 'This build does not match your installed OBS/Qt libraries. Build from source on this distribution.' >&2
    exit 1
fi
destination="${XDG_CONFIG_HOME:-$HOME/.config}/obs-studio/plugins/recording-folder-dock"
install -Dm755 "$plugin" "$destination/bin/64bit/recording-folder-dock.so"
mkdir -p "$destination/data"
cp -R recording-folder-dock/data/. "$destination/data/"
echo "Installed to $destination"
echo 'Open OBS and select Docks > Recording Folder.'

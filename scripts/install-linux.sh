#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
[[ $(uname -s) == Linux ]] || { echo 'This installer is for Linux.' >&2; exit 1; }
if pgrep -x obs >/dev/null; then
    echo 'Close OBS before installing.' >&2
    exit 1
fi
plugin=recording-folder-dock/recording-folder-dock.so
[[ -f "$plugin" && -f recording-folder-dock/data/locale/en-US.ini ]] || { echo 'Extract the Linux package before running this installer.' >&2; exit 1; }
version=$(obs --version)
[[ $version =~ ([0-9]+)\. ]] && (( BASH_REMATCH[1] >= 33 )) || {
    echo 'This package requires OBS 33 or newer. Use plugin v1.4.0 for OBS 31.1-32.' >&2; exit 1;
}
if ! dependencies=$(ldd "$plugin" 2>&1) || grep -q 'not found' <<< "$dependencies"; then
    printf '%s\n' "$dependencies" >&2
    echo 'This build does not match your installed OBS/Qt libraries. Build from source on this distribution.' >&2
    exit 1
fi
base="${XDG_DATA_HOME:-$HOME/.local/share}/obs-studio"
destination="$base/plugins/recording-folder-dock"
legacy="${XDG_CONFIG_HOME:-$HOME/.config}/obs-studio/plugins/recording-folder-dock"
[[ $destination == /* && $legacy == /* ]] || { echo 'XDG paths must be absolute.' >&2; exit 1; }
# Reject symlinked parents so migration cannot modify an unrelated tree.
check_path() {
    local cursor="$1"
    while [[ $cursor != / ]]; do
        [[ ! -L $cursor ]] || { echo "Refusing linked path: $cursor" >&2; exit 1; }
        cursor=$(dirname "$cursor")
    done
    if [[ -d $1 ]] && [[ -n $(find "$1" -type l -print -quit) ]]; then
        echo "Refusing a path containing links: $1" >&2; exit 1
    fi
}
check_path "$destination"
check_path "$legacy"
backup_root="$base/plugin-backups/recording-folder-dock"
check_path "$backup_root"
mkdir -p "$backup_root"
backup=$(mktemp -d "$backup_root/install-XXXXXXXX")
[[ ! -e $destination ]] || cp -a "$destination" "$backup/previous-install"
[[ ! -e $legacy || $legacy == "$destination" ]] || cp -a "$legacy" "$backup/legacy-install"
install -Dm755 "$plugin" "$destination/recording-folder-dock.so"
mkdir -p "$destination/data"
cp -R recording-folder-dock/data/. "$destination/data/"
cmp "$plugin" "$destination/recording-folder-dock.so"
# Remove only the known old module, leaving any other files and all settings intact.
if [[ $legacy != "$destination" ]]; then
    rm -f -- "$legacy/bin/64bit/recording-folder-dock.so"
else
    rm -f -- "$destination/bin/64bit/recording-folder-dock.so"
fi
echo "Installed to $destination"
echo "Previous plugin files backed up to $backup"
echo 'Open OBS and select Docks > Recording Folder.'

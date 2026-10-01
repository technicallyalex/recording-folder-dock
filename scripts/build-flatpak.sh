#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
version=$(python3 -c 'import json; print(json.load(open("buildspec.json"))["version"])')
arch=$(uname -m)
package="dist/recording-folder-dock-${version}-flatpak-${arch}.flatpak"
[[ ! -e "$package" ]] || { echo "Package already exists: $package" >&2; exit 1; }
flatpak remote-add --user --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
flatpak install --user --noninteractive --include-sdk flathub com.obsproject.Studio
mkdir -p dist
flatpak info --user --show-metadata com.obsproject.Studio > dist/obs-flatpak-metadata.ini
source_dir="$PWD/build_flatpak_source_v$version"
mkdir -p "$source_dir"
git archive HEAD | tar -x -C "$source_dir"
python3 scripts/flatpak-manifest.py "$source_dir" dist/obs-flatpak-metadata.ini dist/flatpak-manifest.json
flatpak-builder --user --install-deps-from=flathub --repo="build_flatpak_repo_v$version" \
    "build_flatpak_v$version" dist/flatpak-manifest.json
flatpak build-bundle --runtime "build_flatpak_repo_v$version" "$package" \
    com.obsproject.Studio.Plugin.RecordingFolderDock stable
echo "Package ready: $package"

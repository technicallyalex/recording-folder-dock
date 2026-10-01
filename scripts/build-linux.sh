#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
[[ $(uname -s) == Linux ]] || { echo 'Run this script on Linux.' >&2; exit 1; }
version=$(python3 -c 'import json; print(json.load(open("buildspec.json"))["version"])')
arch=$(uname -m)
build_dir="build_linux_${arch}_v${version}"
package="dist/recording-folder-dock-${version}-linux-${arch}"
[[ ! -e "$package.tar.gz" && ! -e "$package" ]] || { echo "Package already exists: $package" >&2; exit 1; }
cmake -S . -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=ON
cmake --build "$build_dir" --parallel 2
ctest --test-dir "$build_dir" --output-on-failure
plugin="$build_dir/recording-folder-dock.so"
install -Dm755 "$plugin" "$package/recording-folder-dock/bin/64bit/recording-folder-dock.so"
cp -R data "$package/recording-folder-dock/data"
cp scripts/install-linux.sh README.md TESTING.md LICENSE "$package/"
{
    echo "Recording Folder Dock $version / Linux $arch"
    cat /etc/os-release
    echo 'Build-time OBS and Qt versions:'
    pkg-config --modversion libobs Qt6Core
    echo 'Required shared libraries:'
    ldd "$plugin"
} > "$package/BUILD-INFO.txt"
if ldd "$plugin" | grep -q 'not found'; then
    echo 'Unresolved plugin dependencies.' >&2
    exit 1
fi
tar -czf "$package.tar.gz" -C "$package" .
echo "Package ready: $package.tar.gz"

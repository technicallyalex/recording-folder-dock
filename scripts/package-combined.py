"""Bundle native Windows/Linux and Flatpak packages into one versioned download."""
import hashlib
import json
from pathlib import Path
import sys
import zipfile

version = json.loads(Path("buildspec.json").read_text())["version"]
source, destination = map(Path, sys.argv[1:])
destination.mkdir(parents=True, exist_ok=True)
names = [f"recording-folder-dock-{version}-windows-x64.zip",
         f"recording-folder-dock-{version}-linux-x86_64.tar.gz",
         f"recording-folder-dock-{version}-flatpak-x86_64.flatpak"]
packages = []
for name in names:
    matches = list(source.rglob(name))
    if len(matches) != 1:
        raise SystemExit(f"Expected exactly one {name}; found {len(matches)}")
    packages.append(matches[0])
archive = destination / f"recording-folder-dock-{version}-windows-linux.zip"
with zipfile.ZipFile(archive, "x", zipfile.ZIP_DEFLATED) as bundle:
    for package in packages:
        bundle.write(package, package.name)
    bundle.writestr("INSTALL.txt", "Use the Windows ZIP, native Linux tar.gz, or Flatpak bundle for your OBS installation.\n"
                    "Native Linux binary: Ubuntu 24.04 x86-64, OBS PPA. Other distributions: build from source.\n"
                    "Flatpak: flatpak install --user ./recording-folder-dock-" + version + "-flatpak-x86_64.flatpak\n"
                    "One source/version with separate binaries. See README.md for requirements.\n")
    bundle.write("README.md")
with (destination / f"SHA256SUMS-{version}.txt").open("x") as output:
    for path in [*packages, archive]:
        output.write(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n")
print(archive)

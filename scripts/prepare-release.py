"""Validate separate platform packages and stage a release without overwriting files."""
import hashlib
import json
from pathlib import Path
import shutil
import sys

source, destination = map(Path, sys.argv[1:])
version = json.loads(Path("buildspec.json").read_text(encoding="utf-8"))["version"]
names = [f"recording-folder-dock-{version}-{suffix}" for suffix in (
    "windows-x64.zip", "linux-x86_64.tar.gz", "flatpak-x86_64.flatpak")]
packages = []
for name in names:
    matches = list(source.rglob(name))
    if len(matches) != 1 or matches[0].stat().st_size == 0:
        raise SystemExit(f"Expected one nonempty package: {name}")
    packages.append(matches[0])
destination.mkdir(parents=True, exist_ok=False)
checksums = []
for package in packages:
    target = destination / package.name
    shutil.copyfile(package, target)
    checksums.append(f"{hashlib.sha256(target.read_bytes()).hexdigest()}  {target.name}\n")
(destination / "SHA256SUMS.txt").write_text("".join(checksums), encoding="ascii")

"""Use the installed OBS Flatpak's SDK, rather than a mismatched host Qt build."""
import configparser
import json
from pathlib import Path
import sys

source, metadata, output = map(Path, sys.argv[1:])
config = configparser.ConfigParser(interpolation=None)
config.read(metadata)
sdk = config["Application"]["sdk"]
sdk_name, _, sdk_branch = sdk.split("/")
manifest = {
    "id": "com.obsproject.Studio.Plugin.RecordingFolderDock",
    "branch": "stable",
    "runtime": "com.obsproject.Studio",
    "runtime-version": "stable",
    "sdk": f"{sdk_name}//{sdk_branch}",
    "build-extension": True,
    "separate-locales": False,
    "appstream-compose": False,
    "build-options": {"prefix": "/app/plugins/RecordingFolderDock"},
    "modules": [{
        "name": "recording-folder-dock",
        "buildsystem": "cmake-ninja",
        "builddir": True,
        "config-opts": ["-DCMAKE_BUILD_TYPE=RelWithDebInfo", "-DCMAKE_INSTALL_LIBDIR=lib", "-DBUILD_TESTING=ON"],
        "run-tests": True,
        "sources": [{"type": "dir", "path": str(source.resolve())}]
    }]
}
output.write_text(json.dumps(manifest, indent=2) + "\n")

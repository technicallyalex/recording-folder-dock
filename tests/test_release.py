"""Verify release staging refuses missing, duplicate, empty, or existing assets."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
VERSION = json.loads((ROOT / "buildspec.json").read_text(encoding="utf-8"))["version"]


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.source = Path(self.temp.name) / "packages"
        self.source.mkdir()
        self.output = Path(self.temp.name) / "release"
        for suffix in ("windows-x64.zip", "linux-x86_64.tar.gz", "flatpak-x86_64.flatpak"):
            (self.source / f"recording-folder-dock-{VERSION}-{suffix}").write_bytes(b"package fixture")

    def stage(self):
        return subprocess.run([sys.executable, str(ROOT / "scripts/prepare-release.py"),
                               str(self.source), str(self.output)], cwd=ROOT, capture_output=True)

    def test_separate_packages_and_checksums(self):
        self.assertEqual(self.stage().returncode, 0)
        self.assertEqual(len(list(self.output.iterdir())), 4)
        for line in (self.output / "SHA256SUMS.txt").read_text().splitlines():
            digest, name = line.split("  ")
            self.assertEqual(digest, hashlib.sha256((self.output / name).read_bytes()).hexdigest())
        self.assertNotEqual(self.stage().returncode, 0)

    def test_missing_package(self):
        next(self.source.iterdir()).unlink()
        self.assertNotEqual(self.stage().returncode, 0)
        self.assertFalse(self.output.exists())

    def test_empty_package(self):
        next(self.source.iterdir()).write_bytes(b"")
        self.assertNotEqual(self.stage().returncode, 0)

    def test_duplicate_package(self):
        duplicate = self.source / "duplicate"
        duplicate.mkdir()
        original = next(path for path in self.source.iterdir() if path.is_file())
        (duplicate / original.name).write_bytes(b"duplicate")
        self.assertNotEqual(self.stage().returncode, 0)

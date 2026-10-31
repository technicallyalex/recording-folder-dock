"""Exercise native migration in temporary XDG roots without loading a plugin."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(os.name == "posix", "Linux shell fixture")
class LinuxInstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.package = self.root / "package"
        plugin = self.package / "recording-folder-dock"
        (plugin / "data/locale").mkdir(parents=True)
        (plugin / "recording-folder-dock.so").write_bytes(b"new plugin")
        (plugin / "data/locale/en-US.ini").write_text("locale")
        (self.package / "install-linux.sh").write_bytes((ROOT / "scripts/install-linux.sh").read_bytes())
        self.bin = self.root / "bin"
        self.bin.mkdir()
        for name, body in {"obs": "echo 'OBS Studio 33.0.0'", "ldd": "exit 0", "pgrep": "exit 1"}.items():
            self.command(name, body)
        self.env = dict(os.environ, PATH=f"{self.bin}:{os.environ['PATH']}",
                        XDG_DATA_HOME=str(self.root / "data"), XDG_CONFIG_HOME=str(self.root / "config"))
        self.dest = self.root / "data/obs-studio/plugins/recording-folder-dock"
        self.legacy = self.root / "config/obs-studio/plugins/recording-folder-dock"

    def command(self, name, body):
        path = self.bin / name
        path.write_text("#!/bin/sh\n" + body + "\n")
        path.chmod(0o755)

    def run_installer(self):
        return subprocess.run(["bash", str(self.package / "install-linux.sh")], env=self.env,
                              capture_output=True, text=True)

    def test_migration_and_repeat_install(self):
        old = self.legacy / "bin/64bit/recording-folder-dock.so"
        old.parent.mkdir(parents=True)
        old.write_bytes(b"old plugin")
        other = old.with_name("other.so")
        other.write_bytes(b"other plugin")
        result = self.run_installer()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.dest / "recording-folder-dock.so").read_bytes(), b"new plugin")
        self.assertEqual((self.dest / "data/locale/en-US.ini").read_text(), "locale")
        self.assertFalse(old.exists())
        self.assertTrue(other.exists())
        backups = list((self.root / "data/obs-studio/plugin-backups").rglob("recording-folder-dock.so"))
        self.assertEqual([x.read_bytes() for x in backups], [b"old plugin"])
        self.assertEqual(self.run_installer().returncode, 0)

    def test_old_obs_and_running_obs_leave_install_untouched(self):
        self.command("obs", "echo 'OBS Studio 32.2.1'")
        self.assertNotEqual(self.run_installer().returncode, 0)
        self.assertFalse(self.dest.exists())
        self.command("pgrep", "exit 0")
        self.assertIn("Close OBS", self.run_installer().stderr)

    def test_dependency_failure_and_relative_xdg(self):
        self.command("ldd", "echo 'libobs.so => not found'")
        self.assertNotEqual(self.run_installer().returncode, 0)
        self.assertFalse(self.dest.exists())
        self.command("ldd", "exit 0")
        self.env["XDG_DATA_HOME"] = "relative"
        self.assertIn("absolute", self.run_installer().stderr)

    def test_linked_destination_is_rejected(self):
        self.dest.parent.mkdir(parents=True)
        target = self.root / "unrelated"
        target.mkdir()
        self.dest.symlink_to(target, target_is_directory=True)
        self.assertIn("linked path", self.run_installer().stderr)
        self.assertEqual(list(target.iterdir()), [])

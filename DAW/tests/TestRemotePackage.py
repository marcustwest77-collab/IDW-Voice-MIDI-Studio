"""Regression coverage for isolated Windows remote package verification."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from VerifyRemotePackage import verify_package


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.root = Path(self.folder.name)
        self.worker = self.root / "remote-worker"
        self.worker.mkdir()
        source = Path(__file__).resolve().parent.parent / "network"
        for path in source.glob("*.py"):
            if not path.name.startswith("test_"):
                shutil.copy2(path, self.worker)
        for name, destination in (
                ("OPEN-LOCAL-TEST-CLIENT.cmd", "OPEN-LOCAL-TEST-CLIENT.cmd"),
                ("LOCAL_TEST_SETUP.md", "LOCAL-REMOTE-TEST.txt"),
                ("local-test-setup.example.json", "local-test-setup.example.json")):
            shutil.copy2(source / name, self.root / destination)
        self.openssl = shutil.which("openssl")
        self.assertIsNotNone(self.openssl)

    def test_complete_package(self):
        verify_package(self.root, self.openssl)
        self.assertFalse(list(self.root.rglob("__pycache__")))
        self.assertFalse(list(self.root.rglob("*.key")))

    def test_credentials_and_missing_files_rejected(self):
        key = self.worker / "accidental.key"
        key.write_text("test-only placeholder")
        with self.assertRaises(ValueError):
            verify_package(self.root, self.openssl)
        key.unlink()
        (self.root / "OPEN-LOCAL-TEST-CLIENT.cmd").unlink()
        with self.assertRaises(ValueError):
            verify_package(self.root, self.openssl)

    def test_missing_dependency_has_no_checkout_fallback(self):
        (self.worker / "command_protocol.py").unlink()
        with self.assertRaises(subprocess.CalledProcessError):
            verify_package(self.root, self.openssl)


if __name__ == "__main__":
    unittest.main()

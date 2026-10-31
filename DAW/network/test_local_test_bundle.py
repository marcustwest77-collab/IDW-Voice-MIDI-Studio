"""Creation must never replace existing folders or leave partial credentials."""
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
from create_local_test_bundle import create_bundle


class BundleTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.root = Path(self.folder.name)
        self.executable = self.root / "openssl-test-placeholder"
        self.executable.write_text("never executed")

    def test_existing_folder_is_untouched(self):
        output = self.root / "existing"
        output.mkdir()
        keep = output / "keep.txt"
        keep.write_text("keep")
        with patch("create_local_test_bundle.subprocess.run") as run:
            with self.assertRaises(FileExistsError):
                create_bundle(output, str(self.executable))
            run.assert_not_called()
        self.assertEqual(keep.read_text(), "keep")

    def test_invalid_arguments_do_not_create_or_execute(self):
        output = self.root / "invalid"
        with patch("create_local_test_bundle.subprocess.run") as run:
            for options in ({"peer": "bad peer"}, {"tls_port": True}, {"tls_port": 80}):
                with self.assertRaises(ValueError):
                    create_bundle(output, str(self.executable), **options)
            with self.assertRaises(ValueError):
                create_bundle(output, "relative-openssl")
            run.assert_not_called()
        self.assertFalse(output.exists())

    def test_failed_generation_removes_only_its_new_directory(self):
        output = self.root / "new"
        keep = self.root / "keep.txt"
        keep.write_text("keep")
        def failure(*args, **kwargs):
            (output / "partial.key").write_text("partial")
            raise subprocess.TimeoutExpired("openssl", 30)
        with patch("create_local_test_bundle.subprocess.run", side_effect=failure):
            with self.assertRaises(subprocess.TimeoutExpired):
                create_bundle(output, str(self.executable))
        self.assertFalse(output.exists())
        self.assertEqual(keep.read_text(), "keep")


if __name__ == "__main__":
    unittest.main()

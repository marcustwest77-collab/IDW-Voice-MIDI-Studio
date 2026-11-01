"""Exercise packaged remote tools with no source-checkout import fallback."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

REQUIRED = (
    "remote-worker/tls_native_worker.py",
    "remote-worker/local_test_client.py",
    "remote-worker/local_test_client_ui.py",
    "remote-worker/create_local_test_bundle.py",
    "OPEN-LOCAL-TEST-CLIENT.cmd",
    "LOCAL-REMOTE-TEST.txt",
    "local-test-setup.example.json",
)

# -I excludes the working directory, PYTHONPATH and user site packages.
# The only application import directory added is the packaged worker folder.
SMOKE = r'''
import sys
from pathlib import Path
sys.path.insert(0, sys.argv[1])
import tls_native_worker
import local_test_client_ui
from create_local_test_bundle import create_bundle
from local_test_client import load_setup, check_local_setup, build_request
root = Path.cwd() / "temporary-credentials"
setup_path = create_bundle(root, sys.argv[2])
setup = load_setup(setup_path)
assert "No connection" in check_local_setup(setup, str(root / "guest.crt"), str(root / "guest.key"))
assert build_request(setup, epoch=1, sequence=1, request_id="local_request_1",
                     action="select", track_number=1) == b"1|1|1|local_request_1|select|0"
print("Packaged remote imports, certificate generation, setup and request validation passed.")
'''


def verify_package(package, openssl):
    root = Path(package).resolve()
    for relative in REQUIRED:
        if not (root / relative).is_file():
            raise ValueError("Missing packaged file: " + relative)
    worker = root / "remote-worker"
    if any(p.name.startswith("test_") or p.suffix.lower() in (".key", ".pem", ".crt")
           for p in worker.rglob("*") if p.is_file()):
        raise ValueError("Test files or credentials must not be shipped in remote-worker.")
    executable = Path(openssl).resolve()
    if not executable.is_file():
        raise ValueError("A trusted OpenSSL executable is required for this CI check.")
    with tempfile.TemporaryDirectory(prefix="idw-package-check-") as directory:
        result = subprocess.run([sys.executable, "-I", "-B", "-c", SMOKE,
                                 str(worker), str(executable)], cwd=directory,
                                check=True, capture_output=True, text=True, timeout=75)
        print(result.stdout.strip())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package")
    args = parser.parse_args()
    openssl = shutil.which("openssl")
    if not openssl:
        parser.exit(1, "OpenSSL is required on this build runner.\n")
    verify_package(args.package, openssl)


if __name__ == "__main__":
    main()

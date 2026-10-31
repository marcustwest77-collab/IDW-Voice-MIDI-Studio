"""Create disposable localhost credentials using an explicitly chosen OpenSSL."""
import argparse
from pathlib import Path
import shutil
import subprocess
from local_test_client import create_local_setup


def create_bundle(destination, openssl, *, peer="guest", tls_port=49200):
    executable = Path(openssl)
    if not executable.is_absolute() or not executable.is_file():
        raise ValueError("Select an existing absolute path to a trusted OpenSSL executable.")
    # Validate public fields before creating any files or launching a process.
    from local_test_client import ID
    if not isinstance(peer, str) or not ID.fullmatch(peer):
        raise ValueError("Invalid collaborator ID.")
    if type(tls_port) is not int or not 1024 <= tls_port <= 65535:
        raise ValueError("Invalid local port.")
    root = Path(destination).absolute()
    root.mkdir(mode=0o700, parents=False, exist_ok=False)
    try:
        for name, purpose in (("localhost", "serverAuth"), ("guest", "clientAuth")):
            subprocess.run([
                str(executable), "req", "-x509", "-newkey", "rsa:2048", "-nodes",
                "-keyout", str(root / (name + ".key")),
                "-out", str(root / (name + ".crt")), "-days", "7",
                "-subj", "/CN=" + name,
                "-addext", "subjectAltName=DNS:" + name,
                "-addext", "extendedKeyUsage=" + purpose,
            ], check=True, capture_output=True, timeout=30)
            (root / (name + ".key")).chmod(0o600)
        setup = root / "local-test-setup.json"
        create_local_setup(setup, peer=peer, tls_port=tls_port,
                           server_cert=str(root / "localhost.crt"),
                           server_key=str(root / "localhost.key"),
                           client_ca=str(root / "guest.crt"),
                           client_cert=str(root / "guest.crt"),
                           client_key=str(root / "guest.key"))
        (root / "READ-ME.txt").write_text(
            "Disposable local test credentials, valid for 7 days.\n"
            "Keep this folder private. Keys are unencrypted; do not share or upload them.\n"
            "On Windows, use a folder private to your Windows account; POSIX modes do not set Windows ACLs.\n"
            "Load local-test-setup.json in the DAW and local client.\n"
            "In the client select guest.crt and guest.key.\n"
            "No certificates were installed in the system trust store.\n"
            "The setup contains absolute paths: do not move this folder.\n"
            "After testing, stop the DAW test and delete this disposable folder.\n",
            encoding="utf-8")
        return setup
    except Exception:
        # Only the directory created by this call is removed, never an existing one.
        shutil.rmtree(root)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--openssl", required=True, help="Absolute path to your trusted OpenSSL executable")
    parser.add_argument("--output", required=True, help="New private folder in an existing parent; never overwritten")
    parser.add_argument("--peer", default="guest")
    parser.add_argument("--port", type=int, default=49200)
    args = parser.parse_args()
    try:
        setup = create_bundle(args.output, args.openssl, peer=args.peer, tls_port=args.port)
    except FileExistsError:
        parser.exit(1, "Choose a new output folder. Existing folders are never replaced.\n")
    except (OSError, ValueError, subprocess.SubprocessError):
        parser.exit(1, "Could not create test credentials. Check OpenSSL, input values and folder permissions.\n")
    print("Created local test setup: " + str(setup))
    print("Keep the folder private. See READ-ME.txt for client selections and cleanup.")


if __name__ == "__main__":
    main()

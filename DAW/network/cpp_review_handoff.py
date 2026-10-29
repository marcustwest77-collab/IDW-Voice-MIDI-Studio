"""Test-only adapter from authenticated local IPC receiver to C++ host-review fixture.

Never launch or expose this as a production DAW network endpoint. The fixture
uses synthetic session authority; it is NOT the running JUCE workstation.
"""
import subprocess

class CppReviewHandoff:
    def __init__(self, fixture_path):
        if not fixture_path:
            raise ValueError("C++ fixture required")
        self._process=subprocess.Popen([fixture_path], stdin=subprocess.PIPE,
             stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1)

    def accept(self, wire):
        if self._process.poll() is not None or not isinstance(wire,str) or "\n" in wire:
            return False
        try:
            self._process.stdin.write(wire+"\n")
            self._process.stdin.flush()
            return self._process.stdout.readline().strip()=="REVIEW"
        except (OSError, ValueError):
            return False

    def close(self):
        if self._process.poll() is None:
            self._process.stdin.close()
            try:
                self._process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self._process.kill()
                self._process.wait(timeout=3)
        self._process.stdout.close()
        self._process.stderr.close()

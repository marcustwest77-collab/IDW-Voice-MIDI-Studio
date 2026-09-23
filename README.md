# IDW Voice MIDI Studio V6 candidate

Local source candidate; no V6 Windows binary has been validated yet.

Read [V6 upgrade and status](docs/V6-UPGRADE.md) and open START-HERE.html.

The Studio Instrument is native C++/JUCE. Audio Lab is a separate Python 3.10 companion. Optional transcription installs Basic Pitch; cloud voice conversion requires an existing Kits account and voice model. The FL controller script routes notes after manual setup.

Build native Windows application: BUILD-WINDOWS.cmd.
Run independent companion tests: `python -m unittest discover -s Tests -p "test_*.py" -v`.
Proposed workflow: `.github/workflows/windows-v6.yml`.

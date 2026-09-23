@echo off
cd /d "%~dp0"
echo Downloads the local transcription engine and dependencies. No recordings are uploaded.
py -3.10 -m venv .venv
if errorlevel 1 goto fail
".venv\Scripts\python.exe" -m pip install -r requirements-transcription.txt
if errorlevel 1 goto fail
echo Ready. Open Run-Audio-Lab.cmd.
pause
exit /b 0
:fail
echo Setup failed. Python 3.10 with Tkinter and the Python launcher is required.
pause
exit /b 1

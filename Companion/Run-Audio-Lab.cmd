@echo off
cd /d "%~dp0"
if exist ".venv\Scripts\python.exe" (
  ".venv\Scripts\python.exe" audio_lab.py
) else (
  py -3.10 audio_lab.py
)
if errorlevel 1 pause

@echo off
cd /d "%~dp0"
powershell.exe -NoProfile -File "%~dp0scripts\build-windows.ps1"
if errorlevel 1 echo Build failed. Read the message above and START-HERE.html.
pause

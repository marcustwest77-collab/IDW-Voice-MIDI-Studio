@echo off
setlocal
cd /d "%~dp0"
where py >nul 2>nul
if not errorlevel 1 (
  py -3 "%~dp0remote-worker\local_test_client_ui.py"
) else (
  python "%~dp0remote-worker\local_test_client_ui.py"
)
if errorlevel 1 (
  echo The test client could not start. Install a trusted Python 3 distribution with tkinter enabled.
  pause
)

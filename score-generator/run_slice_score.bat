@echo off
rem Opens the "Composing by slices" score generator window. Double-click this file.
cd /d "%~dp0"

where pyw >nul 2>nul
if %errorlevel%==0 (
    start "" pyw slice_score_gui.py
    exit /b
)
where pythonw >nul 2>nul
if %errorlevel%==0 (
    start "" pythonw slice_score_gui.py
    exit /b
)
where python >nul 2>nul
if %errorlevel%==0 (
    python slice_score_gui.py
    pause
    exit /b
)

echo Python was not found on this computer's PATH.
echo Install Python from https://www.python.org (tick "Add python.exe to PATH"), or open slice_score_gui.py in IDLE and press F5.
pause

@echo off
title Smart Motor Health Monitoring Server
color 0A

echo ============================================
echo   Smart Motor Health Monitoring System
echo   Starting Server...
echo ============================================
echo.

:: Navigate to the python server directory
cd /d "%~dp0vibration_project\python_server"

:: Check if virtual environment exists
set "ROOT_PYTHON=%~dp0.venv\Scripts\python.exe"
set "SERVER_PYTHON=.venv\Scripts\python.exe"
if exist "%ROOT_PYTHON%" (
    echo [INFO] Using root venv Python: "%ROOT_PYTHON%"
) else (
    echo [WARN] Root venv Python not found. Falling back to system Python.
)

set "RECREATE_SERVER_VENV=0"
if exist "%SERVER_PYTHON%" (
    setlocal enabledelayedexpansion
    for /f "usebackq delims=" %%V in ('"%SERVER_PYTHON%" -c "import sys; print(sys.version_info[0], sys.version_info[1])"') do set "SERVER_VER=%%V"
    endlocal & set "SERVER_VER=%SERVER_VER%"
    echo [INFO] Existing server venv Python version: %SERVER_VER%
    if not "%SERVER_VER%"=="3 11" (
        echo [WARN] Server virtual environment Python mismatch. Recreating with root Python.
        set "RECREATE_SERVER_VENV=1"
    )
) else (
    set "RECREATE_SERVER_VENV=1"
)

if "%RECREATE_SERVER_VENV%"=="1" (
    if exist ".venv" (
        echo [INFO] Removing old server virtual environment...
        rmdir /s /q ".venv"
    )
    echo [INFO] Creating server virtual environment...
    if exist "%ROOT_PYTHON%" (
        "%ROOT_PYTHON%" -m venv .venv
    ) else (
        python -m venv .venv
    )
    echo Installing dependencies...
    call .venv\Scripts\activate.bat
    pip install -r requirements.txt
    echo.
    echo Virtual environment created and dependencies installed.
    echo.
) else (
    call .venv\Scripts\activate.bat
    echo [OK] Server virtual environment activated.
)

:: Check if model file exists
if not exist "motor_fault_model.pkl" (
    echo [ERROR] motor_fault_model.pkl not found!
    echo Please train the model first.
    pause
    exit /b 1
)

echo [OK] Model file found.
echo.
echo ============================================
echo   Dashboard: http://localhost:5000
echo   Press Ctrl+C to stop the server
echo ============================================
echo.

:: Run the Flask server
python server.py

:: If server stops, keep window open
echo.
echo Server stopped.
pause

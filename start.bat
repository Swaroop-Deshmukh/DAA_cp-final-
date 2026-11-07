@echo off
title Adaptive CDN Traffic Router - Control Console
color 0B

echo ===============================================================================
echo                ADAPTIVE CDN TRAFFIC ROUTER - CONTROL CONSOLE
echo ===============================================================================
echo.

:: Navigate to project root directory
cd /d "%~dp0"

:: 1. Check Python installation
echo [*] Checking Python environment...
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Python is not found in your system PATH!
    echo Please install Python 3.10+ and ensure 'Add Python to PATH' is checked.
    pause
    exit /b 1
)

:: 2. Check C++ engine binary
echo [*] Checking C++17 routing engine binary...
if not exist "cpp-engine\build\Release\cdn_router.exe" (
    echo [!] cdn_router.exe not found in Release folder.
    echo [*] Checking debug folder...
    if exist "cpp-engine\build\Debug\cdn_router.exe" (
        echo [*] Found Debug build. Copying to Release...
        mkdir "cpp-engine\build\Release" 2>nul
        copy "cpp-engine\build\Debug\cdn_router.exe" "cpp-engine\build\Release\cdn_router.exe" >nul
    ) else (
        echo [!] Compiling C++ engine from source...
        set "CMAKE_PATH=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        if exist "%CMAKE_PATH%" (
            cd cpp-engine
            "%CMAKE_PATH%" -B build -S .
            "%CMAKE_PATH%" --build build --config Release
            cd ..
        ) else (
            cmake -B cpp-engine\build -S cpp-engine
            cmake --build cpp-engine\build --config Release
        )
    )
)

if not exist "cpp-engine\build\Release\cdn_router.exe" (
    echo [WARNING] Could not find or build cpp-engine\build\Release\cdn_router.exe
    echo Proceeding, but backend may report C++ engine offline.
) else (
    echo [OK] C++17 routing engine binary verified.
)

:: 3. Check Python dependencies
echo [*] Checking Python backend dependencies...
python -c "import fastapi, uvicorn, pydantic, httpx" >nul 2>&1
if %errorlevel% neq 0 (
    echo [*] Installing required packages from backend\requirements.txt...
    pip install -r backend\requirements.txt
) else (
    echo [OK] Python dependencies verified.
)

echo.
echo ===============================================================================
echo  Launching Adaptive CDN NOC Server:
echo    - Operations Dashboard : http://127.0.0.1:8000/
echo    - Swagger REST API     : http://127.0.0.1:8000/docs
echo ===============================================================================
echo.
echo Starting web server and opening dashboard in your browser...
echo (Press CTRL+C anytime in this window to stop the server)
echo.

:: Automatically open browser after 2 seconds in background
start "" cmd /c "timeout /t 2 /nobreak >nul && start http://127.0.0.1:8000/"

:: Launch FastAPI Uvicorn Server
python -m uvicorn backend.app:app --host 127.0.0.1 --port 8000

pause


@echo off
REM ===========================================================
REM Clean Build with Fixed windows_fix.h
REM ===========================================================

cd /d "%~dp0"

echo.
echo ============================================================
echo Clean Build - Qt 6.8+ Windows Header Fix
echo ============================================================
echo.

REM Step 1: Delete build directories
echo [STEP 1] Cleaning build directories...
if exist "build_windows" (
    rmdir /S /Q build_windows
    echo   Removed: build_windows
)
if exist "build" (
    rmdir /S /Q build
    echo   Removed: build
)
mkdir build_windows
echo   Created: build_windows
echo.

REM Step 2: CMake Configuration
echo ============================================================
echo [STEP 2] Running CMake Configuration
echo ============================================================
cd build_windows
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release ..
if errorlevel 1 (
    echo.
    echo ❌ CMAKE CONFIGURATION FAILED
    cd ..
    pause
    exit /b 1
)
echo ✅ CMake configuration SUCCESS
cd ..

echo.
echo ============================================================
echo [STEP 3] Building with MSBuild
echo ============================================================
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" build_windows\batch_processing_plugin.vcxproj /p:Configuration=Release /m /v:minimal

if errorlevel 1 (
    echo.
    echo ============================================================
    echo ❌ BUILD FAILED - Check errors above
    echo ============================================================
    pause
    exit /b 1
)

echo.
echo ============================================================
echo ✅ BUILD SUCCESSFUL!
echo ============================================================
echo.
echo Output: build_windows\Release\BatchProcessing.brx
echo.
dir build_windows\Release\BatchProcessing.brx
echo.
pause

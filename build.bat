@echo off
REM Build the FFPR native screen reader (hid.dll proxy). Builds ALWAYS go through this script.
REM Nothing is written to any game folder: installing is deploy.bat, a separate step.
setlocal
cd /d "%~dp0"

set "CMAKE=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%CMAKE%" (
    echo ERROR: bundled CMake not found at:
    echo   %CMAKE%
    exit /b 1
)
if not exist build mkdir build

echo Configuring...
"%CMAKE%" -S . -B build -A x64 > build_log.txt 2>&1
if errorlevel 1 (
    echo Configure FAILED - see build_log.txt
    exit /b 1
)

echo Building...
set "CLEAN_BUILD="
if /I "%~1"=="--clean" set "CLEAN_BUILD=--clean-first"
"%CMAKE%" --build build --config Release %CLEAN_BUILD% -- /m /v:minimal >> build_log.txt 2>&1
if errorlevel 1 (
    echo Build FAILED - see build_log.txt
    exit /b 1
)
if not exist "out\hid.dll" (
    echo Build reported success but out\hid.dll is missing.
    exit /b 1
)

echo Checking exports and imports...
python tools\import_check.py out\hid.dll
if errorlevel 1 exit /b 1

echo.
echo Build OK: out\hid.dll
for %%F in ("out\hid.dll") do echo   %%~zF bytes
echo NOT deployed. Run smoketest.bat, then deploy.bat.
endlocal

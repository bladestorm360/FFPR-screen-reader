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
"%CMAKE%" --build build --config Release %CLEAN_BUILD% -- /m:4 /nr:false /v:minimal >> build_log.txt 2>&1
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

echo Checking that hooks stack with MelonLoader / BepInEx style hooks...
"out\checks\coexist_check.exe" > build_check\coexist_report.txt 2>&1
if errorlevel 1 (
    echo Coexistence check FAILED - see build_check\coexist_report.txt
    exit /b 1
)

echo Checking the baked-map router against FF2 and FF1 routes known from the dumps...
"out\checks\route_check.exe" "data\ff2\maps.bin" "data\ff1\maps.bin" > build_check\route_report.txt 2>&1
if errorlevel 1 (
    echo Route check FAILED - see build_check\route_report.txt
    exit /b 1
)

echo Checking every game-code name against each game's dump.cs...
"out\checks\autowalk_check.exe" > build_check\autowalk_report.txt 2>&1
if errorlevel 1 (
    echo Autowalk check FAILED - see build_check\autowalk_report.txt
    exit /b 1
)
"out\checks\mapstate_check.exe" > build_check\mapstate_report.txt 2>&1
if errorlevel 1 (
    echo Map-state check FAILED - see build_check\mapstate_report.txt
    exit /b 1
)
"out\checks\frame_check.exe" > build_check\frame_report.txt 2>&1
if errorlevel 1 (
    echo Scheduler check FAILED - see build_check\frame_report.txt
    exit /b 1
)
"out\checks\battle_results_check.exe" > build_check\battle_results_report.txt 2>&1
if errorlevel 1 (
    echo Battle results check FAILED - see build_check\battle_results_report.txt
    exit /b 1
)
"out\checks\prism_check.exe" > build_check\prism_report.txt 2>&1
if errorlevel 1 (
    echo Prism check FAILED - see build_check\prism_report.txt
    exit /b 1
)
python tools\prism_package_check.py > build_check\prism_package_report.txt 2>&1
if errorlevel 1 (
    echo Prism package check FAILED - see build_check\prism_package_report.txt
    exit /b 1
)
python tools\prism_dependency_check.py > build_check\prism_dependency_report.txt 2>&1
if errorlevel 1 (
    echo Prism dependency check FAILED - see build_check\prism_dependency_report.txt
    exit /b 1
)
"out\checks\modal_input_check.exe" > build_check\modal_input_report.txt 2>&1
if errorlevel 1 (
    echo Modal input check FAILED - see build_check\modal_input_report.txt
    exit /b 1
)
"out\checks\npc_names_check.exe" > build_check\npc_names_report.txt 2>&1
if errorlevel 1 (
    echo NPC naming check FAILED - see build_check\npc_names_report.txt
    exit /b 1
)
for %%G in (ff1 ff2 ff3 ff4 ff5 ff6) do (
    "out\checks\npc_label_audit_check.exe" %%G > build_check\%%G_npc_labels_report.txt 2>&1
    if errorlevel 1 (
        echo NPC label audit FAILED for %%G - see build_check\%%G_npc_labels_report.txt
        exit /b 1
    )
)
python tools\name_check.py
if errorlevel 1 (
    echo Name check FAILED - see build_check\name_report.txt
    exit /b 1
)
REM Report only: a few names are optional by design (FF1's default when undefined).
python tools\wiring_check.py

echo.
echo Build OK: out\hid.dll
for %%F in ("out\hid.dll") do echo   %%~zF bytes
echo NOT deployed. Run smoketest.bat, then deploy.bat.
endlocal

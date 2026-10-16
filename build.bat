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
python tools\openal_dependency.py check
if errorlevel 1 exit /b 1
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
"out\checks\timing_check.exe" > build_check\timing_report.txt 2>&1
if errorlevel 1 (
    echo Timing diagnostics check FAILED - see build_check\timing_report.txt
    exit /b 1
)
"out\checks\coexist_check.exe" > build_check\coexist_report.txt 2>&1
if errorlevel 1 (
    echo Coexistence check FAILED - see build_check\coexist_report.txt
    exit /b 1
)

echo Checking the baked-map router against FF2 and FF1 routes known from the dumps...
"out\checks\map_exit_check.exe" > build_check\map_exit_report.txt 2>&1
if errorlevel 1 (
    echo Map exit check FAILED - see build_check\map_exit_report.txt
    exit /b 1
)
"out\checks\route_check.exe" "data\ff2\maps.bin" "data\ff1\maps.bin" > build_check\route_report.txt 2>&1
if errorlevel 1 (
    echo Route check FAILED - see build_check\route_report.txt
    exit /b 1
)
"out\checks\route_worker_check.exe" > build_check\route_worker_report.txt 2>&1
if errorlevel 1 (
    echo Route worker check FAILED - see build_check\route_worker_report.txt
    exit /b 1
)

"out\checks\puzzle_guide_check.exe" > build_check\puzzle_guide_report.txt 2>&1
if errorlevel 1 (
    echo Puzzle guide check FAILED - see build_check\puzzle_guide_report.txt
    exit /b 1
)
"out\checks\fade_ready_check.exe" > build_check\fade_ready_report.txt 2>&1
if errorlevel 1 (
    echo Fade readiness check FAILED - see build_check\fade_ready_report.txt
    exit /b 1
)
"out\checks\trap_route_check.exe" > build_check\trap_route_report.txt 2>&1
if errorlevel 1 (
    echo Trap routing check FAILED - see build_check\trap_route_report.txt
    exit /b 1
)
"out\checks\transition_route_check.exe" > build_check\transition_route_report.txt 2>&1
if errorlevel 1 (
    echo Transition route check FAILED - see build_check\transition_route_report.txt
    exit /b 1
)
"out\checks\event_semantics_check.exe" > build_check\event_semantics_report.txt 2>&1
if errorlevel 1 (
    echo Event semantics check FAILED - see build_check\event_semantics_report.txt
    exit /b 1
)

"out\checks\teleport_check.exe" > build_check\teleport_report.txt 2>&1
if errorlevel 1 (
    echo Teleport check FAILED - see build_check\teleport_report.txt
    exit /b 1
)

echo Checking every game-code name against each game's dump.cs...
"out\checks\autowalk_check.exe" > build_check\autowalk_report.txt 2>&1
if errorlevel 1 (
    echo Autowalk check FAILED - see build_check\autowalk_report.txt
    exit /b 1
)
"out\checks\autowalk_native_check.exe" > build_check\autowalk_native_report.txt 2>&1
if errorlevel 1 (
    echo Autowalk native input check FAILED - see build_check\autowalk_native_report.txt
    exit /b 1
)
"out\checks\active_controller_check.exe" > build_check\active_controller_report.txt 2>&1
if errorlevel 1 (
    echo Active controller check FAILED - see build_check\active_controller_report.txt
    exit /b 1
)
"out\checks\autowalk_steering_check.exe" > build_check\autowalk_steering_report.txt 2>&1
if errorlevel 1 (
    echo Native steering check FAILED - see build_check\autowalk_steering_report.txt
    exit /b 1
)
"out\checks\bindings_check.exe" > build_check\bindings_report.txt 2>&1
if errorlevel 1 (
    echo Input binding check FAILED - see build_check\bindings_report.txt
    exit /b 1
)
"out\checks\equipment_party_check.exe" > build_check\equipment_party_report.txt 2>&1
if errorlevel 1 (
    echo Equipment and party check FAILED - see build_check\equipment_party_report.txt
    exit /b 1
)
"out\checks\mapstate_check.exe" > build_check\mapstate_report.txt 2>&1
if errorlevel 1 (
    echo Map-state check FAILED - see build_check\mapstate_report.txt
    exit /b 1
)
"out\checks\mapstate_native_check.exe" > build_check\mapstate_native_report.txt 2>&1
if errorlevel 1 (
    echo Native map-state check FAILED - see build_check\mapstate_native_report.txt
    exit /b 1
)
"out\checks\frame_check.exe" > build_check\frame_report.txt 2>&1
if errorlevel 1 (
    echo Scheduler check FAILED - see build_check\frame_report.txt
    exit /b 1
)
"out\checks\log_worker_check.exe" > build_check\log_worker_report.txt 2>&1
if errorlevel 1 (
    echo Logger worker check FAILED - see build_check\log_worker_report.txt
    exit /b 1
)
"out\checks\audio_worker_check.exe" > build_check\audio_worker_report.txt 2>&1
if errorlevel 1 (
    echo Audio worker check FAILED - see build_check\audio_worker_report.txt
    exit /b 1
)
"out\checks\soundscape_check.exe" > build_check\soundscape_report.txt 2>&1
if errorlevel 1 (
    echo Soundscape check FAILED - see build_check\soundscape_report.txt
    exit /b 1
)
"out\checks\beacon_check.exe" > build_check\beacon_report.txt 2>&1
if errorlevel 1 (
    echo Beacon mode check FAILED - see build_check\beacon_report.txt
    exit /b 1
)

"out\checks\sound_player_check.exe" > build_check\sound_player_report.txt 2>&1
if errorlevel 1 (
    echo Cue producer check FAILED - see build_check\sound_player_report.txt
    exit /b 1
)
"out\checks\controller_worker_check.exe" > build_check\controller_worker_report.txt 2>&1
if errorlevel 1 (
    echo Controller worker check FAILED - see build_check\controller_worker_report.txt
    exit /b 1
)
"out\checks\controller_worker_check.exe" failure >> build_check\controller_worker_report.txt 2>&1
if errorlevel 1 (
    echo Controller failure check FAILED - see build_check\controller_worker_report.txt
    exit /b 1
)
"out\checks\battle_context_check.exe" > build_check\battle_context_report.txt 2>&1
if errorlevel 1 (
    echo Battle context check FAILED - see build_check\battle_context_report.txt
    exit /b 1
)
"out\checks\battle_entry_check.exe" > build_check\battle_entry_report.txt 2>&1
if errorlevel 1 (
    echo Battle entry check FAILED - see build_check\battle_entry_report.txt
    exit /b 1
)
"out\checks\battle_formation_check.exe" > build_check\battle_formation_report.txt 2>&1
if errorlevel 1 (
    echo Battle formation check FAILED - see build_check\battle_formation_report.txt
    exit /b 1
)
"out\checks\battle_focus_check.exe" > build_check\battle_focus_report.txt 2>&1
if errorlevel 1 (
    echo Battle focus check FAILED - see build_check\battle_focus_report.txt
    exit /b 1
)
"out\checks\party_esper_check.exe" > build_check\party_esper_report.txt 2>&1
if errorlevel 1 (
    echo Party and Esper check FAILED - see build_check\party_esper_report.txt
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
"out\checks\speech_worker_check.exe" > build_check\speech_worker_report.txt 2>&1
if errorlevel 1 (
    echo Speech worker check FAILED - see build_check\speech_worker_report.txt
    exit /b 1
)
"out\checks\navigation_speech_check.exe" > build_check\navigation_speech_report.txt 2>&1
if not "%errorlevel%"=="0" (
    echo Navigation speech check FAILED - see build_check\navigation_speech_report.txt
    exit /b 1
)
"out\checks\browse_route_check.exe" > build_check\browse_route_report.txt 2>&1
if errorlevel 1 (
    echo Browse route check FAILED - see build_check\browse_route_report.txt
    exit /b 1
)
"out\checks\event_tracking_check.exe" > build_check\event_tracking_report.txt 2>&1
if errorlevel 1 (
    echo Event tracking check FAILED - see build_check\event_tracking_report.txt
    exit /b 1
)
"out\checks\modal_speech_check.exe" > build_check\modal_speech_report.txt 2>&1
if not "%errorlevel%"=="0" (
    echo Modal speech check FAILED - see build_check\modal_speech_report.txt
    exit /b 1
)
"out\checks\title_prompt_check.exe" > build_check\title_prompt_report.txt 2>&1
if errorlevel 1 (
    echo Title prompt check FAILED - see build_check\title_prompt_report.txt
    exit /b 1
)
"out\checks\settings_worker_check.exe" > build_check\settings_worker_report.txt 2>&1
if errorlevel 1 (
    echo Settings worker check FAILED - see build_check\settings_worker_report.txt
    exit /b 1
)
"out\checks\entity_filter_check.exe" > build_check\entity_filter_report.txt 2>&1
if errorlevel 1 (
    echo Entity filter check FAILED - see build_check\entity_filter_report.txt
    exit /b 1
)
"out\checks\entity_scanner_check.exe" > build_check\entity_scanner_report.txt 2>&1
if not "%errorlevel%"=="0" (
    echo Entity scanner check FAILED - see build_check\entity_scanner_report.txt
    exit /b 1
)
"out\checks\transition_input_check.exe" > build_check\transition_input_report.txt 2>&1
if errorlevel 1 (
    echo Transition input check FAILED - see build_check\transition_input_report.txt
    exit /b 1
)
"out\checks\navigation_worker_check.exe" > build_check\navigation_worker_report.txt 2>&1
if not "%errorlevel%"=="0" (
    echo Navigation worker check FAILED - see build_check\navigation_worker_report.txt
    exit /b 1
)
"out\checks\navigation_perf_check.exe" > build_check\navigation_perf_report.txt 2>&1
if not "%errorlevel%"=="0" (
    echo Navigation performance check FAILED - see build_check\navigation_perf_report.txt
    exit /b 1
)
"out\checks\component_cache_check.exe" > build_check\component_cache_report.txt 2>&1
if errorlevel 1 (
    echo Component cache check FAILED - see build_check\component_cache_report.txt
    exit /b 1
)
"out\checks\vehicle_state_check.exe" > build_check\vehicle_state_report.txt 2>&1
if errorlevel 1 (
    echo Vehicle state check FAILED - see build_check\vehicle_state_report.txt
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
python tools\soundscape_assets_check.py > build_check\soundscape_assets_report.txt 2>&1
if errorlevel 1 (
    echo Embedded soundscape asset check FAILED - see build_check\soundscape_assets_report.txt
    exit /b 1
)

echo.
echo Build OK: out\hid.dll
for %%F in ("out\hid.dll") do echo   %%~zF bytes
echo NOT deployed. Run smoketest.bat, then deploy.bat.
endlocal

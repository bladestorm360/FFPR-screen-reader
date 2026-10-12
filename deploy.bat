@echo off
REM Install the native screen reader into every installed FF1-FF6 Pixel Remaster folder,
REM or into one game: deploy.bat ff1   (ff1..ff6).
REM A SEPARATE step from build.bat. Run smoketest.bat first. Never launches a game.
REM MelonLoader, BepInEx and their files are not touched. The old FFn_ScreenReader.dll in Mods is NOT
REM removed by this script; the mod speaks a warning at startup while it is still there.
setlocal
cd /d "%~dp0"
set "STEAM=D:\Games\SteamLibrary\steamapps\common"
if not exist "out\hid.dll" ( echo ERROR: out\hid.dll missing - run build.bat first. & exit /b 1 )

set "ONLY=%~1"
set FAILED=0
call :one ff1 "FINAL FANTASY PR" "FINAL FANTASY.exe"
call :one ff2 "FINAL FANTASY II PR" "FINAL FANTASY II.exe"
call :one ff3 "FINAL FANTASY III PR" "FINAL FANTASY III.exe"
call :one ff4 "FINAL FANTASY IV PR" "FINAL FANTASY IV.exe"
call :one ff5 "FINAL FANTASY V PR" "FINAL FANTASY V.exe"
call :one ff6 "FINAL FANTASY VI PR" "FINAL FANTASY VI.exe"
if not "%FAILED%"=="0" ( echo Some copies FAILED. & exit /b 1 )
echo.
echo Installed hid.dll and SDL3.dll. Prism prerequisite was checked without changes.
echo Log: ffpr_screen_reader.log in each game folder. Uninstall: uninstall.bat
endlocal
exit /b 0

:one
if not "%ONLY%"=="" if /I not "%ONLY%"=="%~1" exit /b 0
set "GAME=%STEAM%\%~2"
if not exist "%GAME%\%~3" ( echo skip %~1: not installed & exit /b 0 )
REM find.exe by full path: a Unix find earlier on PATH (Git Bash) makes this check silently pass.
tasklist /FI "IMAGENAME eq %~3" 2>nul | "%SystemRoot%\System32\find.exe" /I "%~3" >nul
if not errorlevel 1 ( echo ERROR: %~3 is running. Close the game first. & set FAILED=1 & exit /b 0 )
REM Read-only dependency check. Never install, copy, repair or update Prism here.
python tools\prism_dependency.py check %~1 --game-root "%STEAM%"
if errorlevel 1 ( set FAILED=1 & exit /b 0 )
copy /Y "out\hid.dll" "%GAME%\hid.dll" >nul || ( set FAILED=1 & exit /b 0 )
copy /Y "third_party\runtime\SDL3.dll" "%GAME%\SDL3.dll" >nul || ( set FAILED=1 & exit /b 0 )
echo installed %~1: %GAME%
exit /b 0

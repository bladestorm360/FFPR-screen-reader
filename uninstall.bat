@echo off
REM Remove the native screen reader from every FF1-FF6 Pixel Remaster folder.
REM Removes ONLY this mod's own files: hid.dll, its logs and settings.
REM Tolk.dll, nvdaControllerClient64.dll and SDL3.dll stay, because the old MelonLoader
REM screen readers use the same files. MelonLoader and BepInEx are not touched.
REM MinHook.x64.dll is left alone: MinHook is built into hid.dll since 2026-10-07, and a
REM MinHook.x64.dll in the game folder may now belong to another mod.
REM Deleting hid.dll alone disables the mod: the game then loads System32's hid.dll.
REM Keep your settings: uninstall.bat --keep-settings
setlocal
set "STEAM=D:\Games\SteamLibrary\steamapps\common"
set "KEEP=%~1"
call :one "FINAL FANTASY PR"
call :one "FINAL FANTASY II PR"
call :one "FINAL FANTASY III PR"
call :one "FINAL FANTASY IV PR"
call :one "FINAL FANTASY V PR"
call :one "FINAL FANTASY VI PR"
echo Done.
endlocal
exit /b 0

:one
set "GAME=%STEAM%\%~1"
if not exist "%GAME%" exit /b 0
for %%F in (hid.dll ffpr_screen_reader.log ffpr_screen_reader.1.log ffpr_screen_reader.2.log ffpr_screen_reader.3.log ffpr_screen_reader.4.log ffpr_screen_reader.5.log) do (
    if exist "%GAME%\%%F" ( del /F /Q "%GAME%\%%F" && echo removed %~1\%%F )
)
if /I not "%KEEP%"=="--keep-settings" if exist "%GAME%\ffpr_screen_reader.ini" ( del /F /Q "%GAME%\ffpr_screen_reader.ini" && echo removed %~1\ffpr_screen_reader.ini )
exit /b 0

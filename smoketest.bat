@echo off
REM Validate the built DLL WITHOUT launching a game (BDFFHD's smoke test, for hid.dll).
REM A bug in this DLL does not produce a nice error: it stops the GAME from starting. This
REM loads it in a throwaway process, checks all 47 hid.dll exports resolve, calls a forwarded
REM export against the real system DLL, and asserts the boot thread wrote real log lines.
REM Staged inside the build tree, NOT in %TEMP% (an unsigned DLL loaded from %TEMP% is what
REM Defender flagged in BDFFHD on 2026-08-12).
setlocal
cd /d "%~dp0"
if not exist "out\hid.dll" ( echo ERROR: out\hid.dll missing - run build.bat first. & exit /b 1 )
if not exist "out\smoke_host.exe" ( echo ERROR: out\smoke_host.exe missing - run build.bat first. & exit /b 1 )

set "SMOKE=%~dp0build\smoke"
if exist "%SMOKE%" rmdir /s /q "%SMOKE%"
mkdir "%SMOKE%"
copy /Y "out\hid.dll" "%SMOKE%\hid.dll" >nul || ( echo ERROR: could not stage hid.dll & exit /b 1 )
copy /Y "out\smoke_host.exe" "%SMOKE%\smoke_host.exe" >nul || ( echo ERROR: could not stage smoke_host.exe & exit /b 1 )

pushd "%SMOKE%"
"%SMOKE%\smoke_host.exe" hid.dll > "%SMOKE%\host_output.txt" 2>&1
set HOSTRC=%ERRORLEVEL%
popd
type "%SMOKE%\host_output.txt"

if not "%HOSTRC%"=="0" (
    echo FAIL: host exited with %HOSTRC% - the DLL would break the game the same way.
    exit /b 1
)
if not exist "%SMOKE%\ffpr_screen_reader.log" (
    echo FAIL: no log produced. Do NOT deploy.
    exit /b 1
)
for %%F in ("%SMOKE%\ffpr_screen_reader.log") do set SIZE=%%~zF
if %SIZE% LEQ 8 (
    echo FAIL: log is %SIZE% bytes - no lines written. Do NOT deploy.
    exit /b 1
)
echo PASS: DLL loaded, exports forward, boot thread ran, logger wrote %SIZE% bytes.
echo Log: build\smoke\ffpr_screen_reader.log
endlocal
exit /b 0

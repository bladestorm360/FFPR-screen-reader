@echo off
REM Compile-only check of named source files, without touching the shared build tree.
REM For parallel work: each caller passes a unique tag so object files never collide.
REM   tools\compile_check.bat <tag> src\core\foo.cpp src\games\ff1\bar.cpp ...
REM Output: build_check\<tag>\log.txt. Exit code 0 = everything compiled.
setlocal
cd /d "%~dp0.."
set "TAG=%~1"
if "%TAG%"=="" ( echo usage: compile_check.bat tag files... & exit /b 2 )
shift
set "OUT=build_check\%TAG%"
if not exist "%OUT%" mkdir "%OUT%"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
set "FILES="
:collect
if "%~1"=="" goto run
set "FILES=%FILES% %1"
shift
goto collect
:run
cl /nologo /c /std:c++20 /W4 /permissive- /GR- /EHa /utf-8 /MT /O2 /wd4100 /wd4505 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /Isrc /Isrc\core /Ithird_party\sdl3\include /Ithird_party\minhook\include /Ithird_party\tolk /Fo"%OUT%\\" %FILES% > "%OUT%\log.txt" 2>&1
set RC=%ERRORLEVEL%
findstr /C:" error " /C:" warning " "%OUT%\log.txt"
if "%RC%"=="0" ( echo COMPILE OK ) else ( echo COMPILE FAILED - see %OUT%\log.txt )
exit /b %RC%

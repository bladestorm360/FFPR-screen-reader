@echo off
REM Explicit, separate prerequisite install; unchanged verified packages perform zero writes.
REM Never call this from build.bat, deploy.bat, a game launcher, or a mod update.
setlocal
cd /d "%~dp0"
python tools\prism_dependency.py install %*
exit /b %ERRORLEVEL%

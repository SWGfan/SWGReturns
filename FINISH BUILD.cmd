@echo off
rem ---------------------------------------------------------------
rem  Run this AFTER "SETUP EVERYTHING.cmd" stops with a build error.
rem
rem  The repo's bench client (core3client) does not compile against
rem  the pinned engine3, and it aborts make before the server links.
rem  This turns it off with upstream's own switch and finishes the
rem  build, reusing everything already compiled.
rem ---------------------------------------------------------------
title SWGGenesis - finish the build
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0finish_build.ps1"
echo.
pause

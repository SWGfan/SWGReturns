@echo off
rem ---------------------------------------------------------------
rem  Fixes the buffer-overflow crash (engine3 Time.h off-by-one)
rem  and rebuilds. Takes about 50-65 minutes.
rem ---------------------------------------------------------------
title SWGGenesis - fix the crash and rebuild
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0fix_time_overflow.ps1"
echo.
pause

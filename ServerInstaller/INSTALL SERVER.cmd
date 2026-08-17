@echo off
rem ---------------------------------------------------------------
rem  SWG Genesis - server installer
rem  Just double-click. It will ask for permission to make changes.
rem ---------------------------------------------------------------
title SWG Genesis - server installer
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install_server.ps1"
echo.
pause

@echo off
title SWGGenesis - reset and push cleanly
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0repush.ps1"
echo.
pause

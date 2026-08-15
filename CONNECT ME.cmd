@echo off
rem ---------------------------------------------------------------
rem  Double-click this whenever you cannot connect.
rem  Points the game client at the server's current WSL address and
rem  fixes the galaxy row to match. Run it after every WSL restart.
rem ---------------------------------------------------------------
title SWGGenesis - connect me to the server
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0connect_me.ps1"
echo.
pause

@echo off
rem ---------------------------------------------------------------
rem  Double-click this (admin not required, but harmless).
rem
rem  Turns C:\SWGEmu into the folder you play genesis from:
rem  copies the real genesis client content across, parks the stale
rem  Companion-era overrides, verifies all 57 TRE files, and repoints
rem  the server and client configs.
rem ---------------------------------------------------------------
title SWGGenesis - set up the play folder
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup_client.ps1"
echo.
pause

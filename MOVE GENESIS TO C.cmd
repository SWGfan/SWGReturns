@echo off
rem ---------------------------------------------------------------
rem  Double-click this. It runs move_genesis_to_c.ps1 next to it.
rem  Copies D:\SWGGenesis to C:\SWGGenesis (the SSD).
rem ---------------------------------------------------------------
title Move SWGGenesis to the C: SSD
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0move_genesis_to_c.ps1"
echo.
pause

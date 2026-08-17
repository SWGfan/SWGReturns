@echo off
rem ---------------------------------------------------------------
rem  Run this on the computer that ALREADY has Star Wars Galaxies.
rem  Copies your game files to a USB drive or shared folder so the
rem  new machine has everything ready.
rem ---------------------------------------------------------------
title SWG Genesis - copy game files
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0copy_game_files.ps1"
echo.
pause

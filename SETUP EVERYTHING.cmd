@echo off
rem ---------------------------------------------------------------
rem  RIGHT-CLICK this file and choose "Run as administrator".
rem
rem  Sets up the whole genesis server on a fresh Windows install:
rem  copies the repo to the C: SSD, installs WSL2 + Ubuntu, installs
rem  the Core3 toolchain and MariaDB, creates the genesis database,
rem  and builds core3.
rem
rem  Safe to run again if it stops partway -- it skips finished work.
rem ---------------------------------------------------------------
title SWGGenesis - full server setup
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup_genesis_server.ps1"
echo.
echo  ---------------------------------------------------------------
echo   Window stays open so you can read the result. Press a key when
echo   you are done.
echo  ---------------------------------------------------------------
pause

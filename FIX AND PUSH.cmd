@echo off
rem ---------------------------------------------------------------
rem  Undoes the rejected commit, fixes .gitignore so the 628 MB
rem  object database is excluded, re-stages deliberately, and pushes.
rem  Shows everything first and waits for YES.
rem ---------------------------------------------------------------
title SWGGenesis - fix and push
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0fix_and_push.ps1"
echo.
pause

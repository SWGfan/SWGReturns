@echo off
rem ---------------------------------------------------------------
rem  Commits tonight's work and pushes it to SWGfan/SWGReturns.
rem  Shows you everything first and waits for you to type YES.
rem  Nothing is written until you do.
rem ---------------------------------------------------------------
title SWGGenesis - commit and push
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0commit_and_push.ps1"
echo.
pause

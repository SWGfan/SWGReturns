@echo off
rem ---------------------------------------------------------------
rem  Collects the custom game content, hashes it, and writes the
rem  manifest the launcher reads. Publish the result as a GitHub
rem  Release on SWGfan/ServerInstaller.
rem ---------------------------------------------------------------
title SWG Genesis - build content bundle
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0make_content_bundle.ps1"
echo.
pause

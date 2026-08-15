@echo off
rem ---------------------------------------------------------------
rem  RIGHT-CLICK this file and choose "Run as administrator".
rem
rem  Installs AutoHotkey v2 (which the control panel is written in)
rem  and puts a "SWG Genesis Control Panel" shortcut on your Desktop.
rem ---------------------------------------------------------------
title SWGGenesis - install the control panel
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install_control_panel.ps1"
echo.
pause

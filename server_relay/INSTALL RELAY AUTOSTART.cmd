@echo off
rem ---------------------------------------------------------------
rem  RIGHT-CLICK -> Run as administrator.
rem
rem  Makes the relay start automatically when this computer boots,
rem  so a restart cannot silently leave your server unreachable.
rem  Runs hidden; check server_relay\relay.log to see what it did.
rem ---------------------------------------------------------------
title SWG Genesis - install relay autostart

schtasks /Create /F /TN "SWGGenesis Server Relay" /SC ONSTART /RL HIGHEST /RU SYSTEM ^
  /TR "cmd /c cd /d \"%~dp0\" && python relay.py >> relay-autostart.log 2>&1"

if errorlevel 1 (
    echo.
    echo   Could not create the task. Did you run this as administrator?
) else (
    echo.
    echo   Done. The relay will start with Windows.
    echo   Remove it later with:  schtasks /Delete /TN "SWGGenesis Server Relay" /F
)
echo.
pause

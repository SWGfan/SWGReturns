@echo off
rem ---------------------------------------------------------------
rem  Starts the UDP relay that lets outside players reach the server.
rem
rem  WSL2 puts the server behind its own NAT, and Windows' built-in
rem  port forwarding (netsh portproxy) only handles TCP -- but the
rem  game's login, ping and zone traffic is all UDP. This bridges it.
rem
rem  Leave this window open while the server is running.
rem ---------------------------------------------------------------
title SWG Genesis - server relay

where python >nul 2>&1
if errorlevel 1 (
    echo.
    echo   Python is not installed on this machine.
    echo.
    echo   Install it with:   winget install Python.Python.3.12
    echo   then run this again.
    echo.
    pause
    exit /b 1
)

python "%~dp0relay.py" %*
echo.
echo   Relay stopped.
pause

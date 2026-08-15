@echo off
:: Written by the SWGGenesis control panel -- Backup Schedule.
:: Safe to run by hand (double-click) to test exactly what the
:: scheduled task does.
echo.>> C:\SWGGenesis\backups\backup.log
echo ==== %DATE% %TIME% ====>> C:\SWGGenesis\backups\backup.log
wsl.exe -d Ubuntu-24.04 --exec bash -lc "python3 /mnt/c/SWGGenesis/swggenesis_menu.py backup_all --auto --keep 6 >> /mnt/c/SWGGenesis/backups/backup.log 2>&1"

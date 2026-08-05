@echo off
:: Written by the SWGGenesis control panel -- Backup Schedule.
:: Safe to run by hand (double-click) to test exactly what the
:: scheduled task does.
echo.>> D:\SWGGenesis\backups\backup.log
echo ==== %DATE% %TIME% ====>> D:\SWGGenesis\backups\backup.log
wsl.exe -d Debian --exec bash -lc "python3 /mnt/d/SWGGenesis/swggenesis_menu.py backup_all --auto --keep 6 >> /mnt/d/SWGGenesis/backups/backup.log 2>&1"

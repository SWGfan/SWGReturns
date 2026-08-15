# =====================================================================
#  SWGGenesis control panel — installer
#  Written by SWGReturn (Claude), 2026-08-15.
#
#  The control panel (SWGGenesisControlPanel.ahk) is an AutoHotkey v2
#  script. A fresh Windows has no AutoHotkey, so nothing happens when
#  you double-click it. This installs v2 and makes a Desktop shortcut.
# =====================================================================

$ErrorActionPreference = 'Continue'

$Panel = 'C:\SWGGenesis\SWGGenesisControlPanel.ahk'

function Say($m, $c = 'Gray') { Write-Host $m -ForegroundColor $c }

Clear-Host
Say ''
Say '  ============================================================' Cyan
Say '   SWG Genesis Control Panel — setup' Cyan
Say '  ============================================================' Cyan
Say ''

$isAdmin = ([Security.Principal.WindowsPrincipal] `
            [Security.Principal.WindowsIdentity]::GetCurrent()
           ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Say '  Needs to run as Administrator.' Red
    Say '  Close this, right-click "INSTALL CONTROL PANEL.cmd", and pick' Yellow
    Say '  "Run as administrator".' Yellow
    Say ''
    return
}

# --- 1. AutoHotkey v2 ------------------------------------------------
Say '  [1/3] AutoHotkey v2'

function Find-AHK {
    $candidates = @(
        "$env:ProgramFiles\AutoHotkey\v2\AutoHotkey64.exe",
        "$env:ProgramFiles\AutoHotkey\v2\AutoHotkey32.exe",
        "${env:ProgramFiles(x86)}\AutoHotkey\v2\AutoHotkey64.exe",
        "$env:LOCALAPPDATA\Programs\AutoHotkey\v2\AutoHotkey64.exe"
    )
    foreach ($c in $candidates) { if (Test-Path $c) { return $c } }
    $found = Get-ChildItem -Path @("$env:ProgramFiles\AutoHotkey", "$env:LOCALAPPDATA\Programs\AutoHotkey") `
                           -Filter 'AutoHotkey*64.exe' -Recurse -ErrorAction SilentlyContinue |
             Sort-Object FullName -Descending | Select-Object -First 1
    if ($found) { return $found.FullName }
    return $null
}

$ahk = Find-AHK

if ($ahk) {
    Say ("        already installed: {0}" -f $ahk) Green
}
else {
    $winget = Get-Command winget.exe -ErrorAction SilentlyContinue
    if (-not $winget) {
        Say '        winget is not available on this machine.' Red
        Say '        Install AutoHotkey v2 by hand from https://www.autohotkey.com/' Yellow
        Say '        then run this file again.' Yellow
        Say ''
        return
    }

    Say '        installing via winget (this pulls from the official source)...'
    & winget.exe install --id AutoHotkey.AutoHotkey --exact --silent `
        --accept-package-agreements --accept-source-agreements 2>&1 |
        ForEach-Object { Say ('        ' + $_) DarkGray }

    Start-Sleep -Seconds 3
    $ahk = Find-AHK

    if (-not $ahk) {
        Say '        Could not find AutoHotkey after installing.' Red
        Say '        Try installing it by hand from https://www.autohotkey.com/' Yellow
        Say '        then run this file again.' Yellow
        Say ''
        return
    }
    Say ("        installed: {0}" -f $ahk) Green
}

# --- 2. the panel script itself --------------------------------------
Say ''
Say '  [2/3] Control panel script'

if (-not (Test-Path $Panel)) {
    Say ("        NOT FOUND: {0}" -f $Panel) Red
    Say '        Run "SETUP EVERYTHING.cmd" first — it copies the repo' Yellow
    Say '        onto C:. Then run this again.' Yellow
    Say ''
    return
}
Say ('        found: ' + $Panel) Green

$hits = (Select-String -Path $Panel -Pattern 'D:\\SWGGenesis' -SimpleMatch -ErrorAction SilentlyContinue).Count
if ($hits -gt 0) {
    Say ("        WARNING: still has {0} reference(s) to D:\SWGGenesis" -f $hits) Yellow
} else {
    Say '        paths point at C: — good' Green
}

# --- 3. Desktop shortcut ---------------------------------------------
Say ''
Say '  [3/3] Desktop shortcut'

$desktop  = [Environment]::GetFolderPath('Desktop')
$lnkPath  = Join-Path $desktop 'SWG Genesis Control Panel.lnk'

try {
    $shell = New-Object -ComObject WScript.Shell
    $lnk = $shell.CreateShortcut($lnkPath)
    $lnk.TargetPath       = $ahk
    $lnk.Arguments        = ('"{0}"' -f $Panel)
    $lnk.WorkingDirectory = 'C:\SWGGenesis'
    $lnk.IconLocation     = $ahk + ',0'
    $lnk.Description      = 'Start, stop and rebuild the SWG Genesis server'
    $lnk.Save()
    Say ('        created: ' + $lnkPath) Green
}
catch {
    Say ('        could not create the shortcut: ' + $_.Exception.Message) Yellow
    Say ('        you can still double-click ' + $Panel) Gray
}

# --- done ------------------------------------------------------------
Say ''
Say '  ============================================================' Cyan
Say '   Done. "SWG Genesis Control Panel" is on your Desktop.' Green
Say '  ============================================================' Cyan
Say ''
Say '  Note: the panel drives the server through WSL, so its buttons' Gray
Say '  only do anything once SETUP EVERYTHING.cmd has finished' Gray
Say '  installing Ubuntu and building core3.' Gray
Say ''

$go = Read-Host '  Open the control panel now? (y/n)'
if ($go -eq 'y') { Start-Process -FilePath $ahk -ArgumentList ('"{0}"' -f $Panel) -WorkingDirectory 'C:\SWGGenesis' }

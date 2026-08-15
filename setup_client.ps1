# =====================================================================
#  Make C:\SWGEmu the folder you play genesis from
#  Written by SWGReturn (Claude), 2026-08-15.
#
#  WHY THIS IS NEEDED
#  C:\SWGEmu is your OLD Companion-era client. It is missing the three
#  aftermath TRE files genesis is built on (aftermath_1, aftermath_NGE,
#  aftermath_house), its companion_patch.tre is a week stale, and its
#  SWGEmu.exe is a different build from the one genesis has been played
#  with. Pointing the server at it as-is would fail at boot.
#
#  WHAT THIS DOES
#   1. Mirrors the working genesis client from the launcher folder into
#      C:\SWGEmu (game content only -- no launcher, screenshots, dumps).
#   2. Moves the stale Companion-era loose override folders aside, into
#      C:\SWGEmu\_old_companion_loose\ . They are MOVED, not deleted.
#   3. Verifies all 57 TRE files the server expects are present.
#   4. Only if that passes, repoints the server's Core3.TrePath at C:.
#   5. Sets the client's login port to genesis (46453).
#
#  Nothing in the launcher folder is modified or deleted.
# =====================================================================

$ErrorActionPreference = 'Continue'

$Launcher = 'D:\Launcher\newreturnbenserver'
$Client   = 'C:\SWGEmu'
$Cfg      = 'C:\SWGGenesis\MMOCoreORB\bin\conf\config-local.lua'
$Log      = 'C:\SWGGenesis\client_setup_log.txt'
$LoginPort = 46453

function Say($m, $c = 'Gray') {
    Write-Host $m -ForegroundColor $c
    try { Add-Content -Path $Log -Value ("[{0}] {1}" -f (Get-Date -Format 'HH:mm:ss'), $m) -ErrorAction SilentlyContinue } catch {}
}
function Head($m) { Say ''; Say ('  == ' + $m + ' ' + ('=' * [Math]::Max(0, 54 - $m.Length))) Cyan }

Clear-Host
Say ''
Say '  ############################################################' Cyan
Say '  #   Set up C:\SWGEmu as the genesis play folder            #' Cyan
Say '  ############################################################' Cyan

if (-not (Test-Path $Launcher)) { Say ("  Launcher folder not found: {0}" -f $Launcher) Red; return }
if (-not (Test-Path $Client))   { New-Item -ItemType Directory -Force -Path $Client | Out-Null }

# =====================================================================
Head 'STEP 1  move the stale Companion loose folders aside'

# These exist in C:\SWGEmu but NOT in the genesis client. They are
# extracted/overridden Companion-era content. Loose files on disk take
# precedence over TRE archives, so leaving them would let old Companion
# assets silently shadow genesis content -- exactly the kind of bug that
# looks like "the server is broken" but is really the client.
$staleDirs = @('appearance', 'clientdata', 'datatables', 'misc', 'object', 'shader', 'terrain')
$attic = Join-Path $Client '_old_companion_loose'

$moved = 0
foreach ($d in $staleDirs) {
    $p = Join-Path $Client $d
    if (Test-Path $p) {
        New-Item -ItemType Directory -Force -Path $attic | Out-Null
        $dest = Join-Path $attic $d
        if (Test-Path $dest) { Remove-Item $dest -Recurse -Force -ErrorAction SilentlyContinue }
        try   { Move-Item $p $dest -Force; Say ("  moved aside: {0}" -f $d) Yellow; $moved++ }
        catch { Say ("  could not move {0}: {1}" -f $d, $_.Exception.Message) Red }
    }
}
if ($moved -eq 0) { Say '  nothing to move (already clean)' Green }
else { Say ("  {0} folder(s) parked in {1}" -f $moved, $attic) Green }

# =====================================================================
Head 'STEP 2  mirror the genesis client content'

$freeGB = [math]::Round((Get-PSDrive C).Free / 1GB, 2)
Say ("  Free on C: {0:N2} GB  (need roughly 2.5 GB)" -f $freeGB)
if ($freeGB -lt 3) { Say '  Not enough room. Stopping.' Red; return }

# Excluded: the launcher app itself, its settings/assets, screenshots,
# crash dumps, backups, installers. Game content only.
$xf = @('screenShot*.jpg', '*.mdmp', '*.bak', '*.bak_laa', '*.genesis-panel.bak',
        'SWGReturnsLauncher.exe', 'SWGEmu_Setup.exe', 'unins000.dat', 'unins000.exe',
        'characterlist_*.txt', 'ui.log', '*.txt')
$xd = @('Assets', 'Data', 'appsettings')

$rc = @($Launcher, $Client, '/E', '/R:2', '/W:5', '/NP', '/NFL', '/NDL', "/LOG+:$Log")
foreach ($f in $xf) { $rc += '/XF'; $rc += $f }
foreach ($d in $xd) { $rc += '/XD'; $rc += $d }

Say '  Copying ~2.4 GB of client content...'
& robocopy.exe @rc | Out-Null
$code = $LASTEXITCODE
if ($code -ge 8) { Say ("  Copy FAILED (robocopy {0})" -f $code) Red; return }
Say ("  Copy OK (robocopy {0})" -f $code) Green

# =====================================================================
Head 'STEP 3  verify every TRE the server expects'

# Read the authoritative list straight out of the server's own config
# rather than hardcoding it -- if TreFiles changes, this follows.
$treFiles = @()
if (Test-Path $Cfg) {
    $txt = Get-Content $Cfg -Raw
    $m = [regex]::Match($txt, 'Core3\.TreFiles\s*=\s*\{(?<body>[^}]*)\}')
    if ($m.Success) {
        $treFiles = [regex]::Matches($m.Groups['body'].Value, '"([^"]+\.tre)"') |
                    ForEach-Object { $_.Groups[1].Value }
    }
}

if ($treFiles.Count -eq 0) {
    Say '  Could not read Core3.TreFiles from config-local.lua.' Red
    Say '  Leaving TrePath pointing at the launcher folder to be safe.' Yellow
    return
}

Say ("  config lists {0} TRE files" -f $treFiles.Count)
$missing = @()
foreach ($t in $treFiles) { if (-not (Test-Path (Join-Path $Client $t))) { $missing += $t } }

if ($missing.Count -gt 0) {
    Say ("  MISSING {0} of them:" -f $missing.Count) Red
    foreach ($t in $missing) { Say ('    ' + $t) Red }
    Say ''
    Say '  NOT repointing TrePath -- the server would fail to boot.' Yellow
    Say '  It stays on the launcher folder, which still works.' Yellow
    return
}
Say '  all present' Green

# =====================================================================
Head 'STEP 4  point the server at C:\SWGEmu'

$before = ([regex]::Match((Get-Content $Cfg -Raw), 'Core3\.TrePath\s*=\s*"([^"]*)"')).Groups[1].Value
Copy-Item $Cfg ($Cfg + '.bak-clientmove') -Force -ErrorAction SilentlyContinue

$txt = Get-Content $Cfg -Raw
$txt = [regex]::Replace($txt, '(Core3\.TrePath\s*=\s*")[^"]*(")', ('${1}/mnt/c/SWGEmu${2}'))
[IO.File]::WriteAllText($Cfg, $txt)

$after = ([regex]::Match((Get-Content $Cfg -Raw), 'Core3\.TrePath\s*=\s*"([^"]*)"')).Groups[1].Value
Say ("  TrePath  {0}" -f $before)
Say ("        -> {0}" -f $after) Green
Say '  (backup: config-local.lua.bak-clientmove)' DarkGray

# =====================================================================
Head 'STEP 5  point the client at the genesis login port'

# The IP itself is deliberately left alone: WSL2 gets a new address on
# every restart, so the control panel's "Play This Server" button is what
# sets it correctly at play time. Only the port is fixed here.
foreach ($f in @('swgemu.cfg', 'swgemu_login.cfg')) {
    $p = Join-Path $Client $f
    if (-not (Test-Path $p)) { Say ("  {0} not found" -f $f) Yellow; continue }
    $t = Get-Content $p -Raw
    if ($t -match 'loginServerPort0\s*=') {
        $t = [regex]::Replace($t, '(loginServerPort0\s*=\s*)\d+', ('${1}' + $LoginPort))
        [IO.File]::WriteAllText($p, $t)
        $now = ([regex]::Match((Get-Content $p -Raw), 'loginServerAddress0\s*=\s*([^\r\n]*)')).Groups[1].Value
        Say ("  {0}: port -> {1}   (address currently {2})" -f $f, $LoginPort, $now.Trim()) Green
    } else {
        Say ("  {0}: no loginServerPort0 line" -f $f) Yellow
    }
}

# =====================================================================
Head 'DONE'
Say '  C:\SWGEmu is now the genesis play folder, on the SSD.' Green
Say ''
Say '  When the server is up, open the control panel and press' Gray
Say '  "Play This Server" -- that fills in the current WSL IP, which' Gray
Say '  changes every time WSL restarts, and updates the galaxy row.' Gray
Say '  Then launch the game from the panel, not from their launcher' Gray
Say '  (the launcher rewrites the cfgs).' Gray
Say ''
Say ("  Log: {0}" -f $Log) DarkGray
Say ''

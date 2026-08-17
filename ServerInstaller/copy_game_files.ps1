# =====================================================================
#  Copy your game files to another computer
#  https://github.com/SWGfan/ServerInstaller
#
#  Run this on the computer that ALREADY has Star Wars Galaxies.
#  It copies your game folder to a USB drive or network location, so
#  the new machine has everything without hunting for it.
#
#  This is your own copy of the game moving between your own machines.
#  The public content bundle deliberately contains only the custom
#  Genesis and Companion System files - the base game belongs to
#  LucasArts and every player supplies their own.
#
#  Why copy rather than point the server at a network share: the
#  server reads the TRE archives constantly while it boots. Over a
#  network that is painfully slow and fails badly if the share drops.
#  They need to be on the host machine's own disk.
# =====================================================================

$ErrorActionPreference = 'Continue'
function Say($m, $c = 'Gray') { Write-Host $m -ForegroundColor $c }

$BASE_TRES = @('bottom.tre','data_texture_00.tre','data_sample_00.tre','patch_00.tre')
function Test-ClientFolder($p) {
    if (-not $p -or -not (Test-Path $p)) { return $false }
    foreach ($t in $BASE_TRES) { if (-not (Test-Path (Join-Path $p $t))) { return $false } }
    return $true
}

Clear-Host
Say ''
Say '  ============================================================' Cyan
Say '   Copy your game files to another computer' Cyan
Say '  ============================================================' Cyan
Say ''

# --- source -----------------------------------------------------------
Say '  Finding your game folder...' Gray
$src = @('C:\SWGEmu','D:\Launcher\newreturnbenserver','C:\StarWarsGalaxies',
         'C:\Program Files (x86)\StarWarsGalaxies') |
       Where-Object { Test-ClientFolder $_ } | Select-Object -First 1

if ($src) {
    Say ("  Found: $src") Green
    if ((Read-Host '  Use this one? (y/n)') -ne 'y') { $src = $null }
}
while (-not (Test-ClientFolder $src)) {
    Add-Type -AssemblyName System.Windows.Forms
    $d = New-Object System.Windows.Forms.FolderBrowserDialog
    $d.Description = 'Select the game folder to copy FROM'
    if ($d.ShowDialog() -ne 'OK') { Say '  Cancelled.' Yellow; return }
    $src = $d.SelectedPath
    if (-not (Test-ClientFolder $src)) { Say '  That folder has no game files in it.' Red }
}

# --- destination ------------------------------------------------------
Say ''
Say '  Now choose where to copy them - a USB drive, external disk,' Gray
Say '  or a folder shared with the other computer.' Gray
Say ''
Add-Type -AssemblyName System.Windows.Forms
$d2 = New-Object System.Windows.Forms.FolderBrowserDialog
$d2.Description = 'Select the destination (USB drive or shared folder)'
if ($d2.ShowDialog() -ne 'OK') { Say '  Cancelled.' Yellow; return }
$dst = Join-Path $d2.SelectedPath 'SWG-GameFiles'

# --- size check -------------------------------------------------------
Say ''
Say '  Measuring (about a minute)...' Gray
$files = Get-ChildItem $src -File -ErrorAction SilentlyContinue
$size  = ($files | Measure-Object Length -Sum).Sum

$root = [System.IO.Path]::GetPathRoot($d2.SelectedPath)
$freeDst = try { (Get-PSDrive ($root[0])).Free } catch { $null }

Say ("  To copy: {0:N1} GB" -f ($size/1GB))
if ($freeDst -ne $null) {
    Say ("  Free at destination: {0:N1} GB" -f ($freeDst/1GB)) $(if ($freeDst -gt $size*1.05) {'Green'} else {'Red'})
    if ($freeDst -lt $size * 1.05) {
        Say ''
        Say '  Not enough room on the destination. Use a bigger drive.' Red
        Say ''
        return
    }
}

Say ''
Say ("  FROM  $src") Gray
Say ("  TO    $dst") Gray
Say ''
if ((Read-Host '  Start copying? (y/n)') -ne 'y') { Say '  Cancelled.' Yellow; return }

# --- copy -------------------------------------------------------------
# Screenshots, crash dumps and character lists are not game data.
Say ''
Say '  Copying. This takes a while on USB - leave it running.' Cyan
$log = Join-Path $env:TEMP 'swg-copy.log'
$args = @($src, $dst, '/E', '/R:2', '/W:5', '/NP', '/NFL', '/NDL', "/LOG:$log",
          '/XF','screenShot*.jpg','*.mdmp','characterlist_*.txt','*.log',
          '/XD','ReShade','reshade-shaders')
& robocopy.exe @args | Out-Null
$rc = $LASTEXITCODE

Say ''
if ($rc -ge 8) {
    Say ("  Copy FAILED (code $rc). See $log") Red
    Say ''
    return
}

# --- verify -----------------------------------------------------------
Say '  Checking everything arrived...' Gray
$bad = @()
foreach ($f in $files) {
    if ($f.Name -match 'screenShot|\.mdmp$|^characterlist_|\.log$') { continue }
    $t = Join-Path $dst $f.Name
    if (-not (Test-Path $t)) { $bad += "$($f.Name) missing" }
    elseif ((Get-Item $t).Length -ne $f.Length) { $bad += "$($f.Name) wrong size" }
}

Say ''
Say '  ============================================================' Cyan
if ($bad.Count -eq 0) {
    Say '   COPY COMPLETE and verified' Green
    Say ''
    Say '   On the other computer:' Cyan
    Say '     1. Plug in the drive (or open the shared folder)' Gray
    Say '     2. Run INSTALL SERVER.cmd' Gray
    Say ("     3. When it asks for your game folder, choose SWG-GameFiles") Gray
    Say ''
    Say '   It copies them to that machine''s own disk - the server' DarkGray
    Say '   cannot read them over a network fast enough to boot.' DarkGray
} else {
    Say ("   {0} problem(s) found:" -f $bad.Count) Red
    $bad | Select-Object -First 10 | ForEach-Object { Say ("     $_") Red }
    Say ''
    Say '   Run this again - robocopy will only redo what is wrong.' Yellow
}
Say '  ============================================================' Cyan
Say ''

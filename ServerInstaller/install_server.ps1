# =====================================================================
#  SWG Genesis - server installer
#  https://github.com/SWGfan/ServerInstaller
#
#  Takes a normal Windows PC with nothing on it and leaves you with a
#  running Star Wars Galaxies server with the Companion System.
#
#  Safe to run again at any point: every stage checks whether it is
#  already done and skips itself. That is also how it survives the
#  reboot Windows asks for after installing WSL.
# =====================================================================

$ErrorActionPreference = 'Continue'

# ---- self-elevate -----------------------------------------------------
# "Right-click, Run as administrator" is the single most misread
# instruction there is. Just ask for it ourselves.
$id = [Security.Principal.WindowsIdentity]::GetCurrent()
if (-not (New-Object Security.Principal.WindowsPrincipal $id).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Start-Process powershell.exe -Verb RunAs -ArgumentList @(
        '-NoProfile','-ExecutionPolicy','Bypass','-File',"`"$PSCommandPath`"") | Out-Null
    return
}

$REPO_URL = 'https://github.com/SWGfan/SWGReturns.git'
$DISTRO   = 'Ubuntu-24.04'
$STATE    = Join-Path $env:ProgramData 'SWGGenesisInstaller\state.json'

function Say($m, $c = 'Gray') { Write-Host $m -ForegroundColor $c }
function Head($n, $of, $t) {
    Say ''
    Say ("  [$n/$of]  $t") Cyan
    Say ('  ' + ('-' * 56)) DarkGray
}
function Fail($m) { Say ''; Say "  PROBLEM: $m" Red; Say '' }

function Load-State {
    if (Test-Path $STATE) { try { return Get-Content $STATE -Raw | ConvertFrom-Json } catch {} }
    return [pscustomobject]@{ client = ''; install = ''; dbpass = ''; ports = $null }
}
function Save-State($s) {
    New-Item -ItemType Directory -Force -Path (Split-Path $STATE) | Out-Null
    $s | ConvertTo-Json | Set-Content $STATE
}

Clear-Host
Say ''
Say '  ================================================================' Cyan
Say '     SWG GENESIS  -  server installer' Cyan
Say '     Companion System included' Cyan
Say '  ================================================================' Cyan
Say ''

$state = Load-State

# =====================================================================
Head 1 8 'Checking your computer'

# --- virtualisation: the one thing no installer can fix for you -------
$virt = $true
try {
    $cs = Get-CimInstance Win32_ComputerSystem
    $virt = -not ($cs.HypervisorPresent -eq $false -and
                 (Get-CimInstance Win32_Processor).VirtualizationFirmwareEnabled -eq $false)
} catch {}

if (-not $virt) {
    $mb = try { (Get-CimInstance Win32_BaseBoard).Manufacturer } catch { 'your' }
    $cpu = try { (Get-CimInstance Win32_Processor).Manufacturer } catch { '' }
    $setting = if ($cpu -match 'AMD') { '"SVM Mode"' } else { '"Intel Virtualization Technology" or "Intel VT-x"' }
    Fail 'Virtualization is turned off in your computer''s BIOS.'
    Say "  The server runs inside Linux, which needs this switched on." Gray
    Say ''
    Say "  On your $mb board, look for $setting" Yellow
    Say '  and set it to Enabled.' Yellow
    Say ''
    $go = Read-Host '  Restart into BIOS settings now? (y/n)'
    if ($go -eq 'y') { shutdown /r /fw /t 5; Say '  Restarting...' Yellow }
    return
}
Say '  Virtualization: on' Green

# --- disk ---------------------------------------------------------------
$free = [math]::Round((Get-PSDrive C).Free / 1GB, 1)
Say ("  Free space on C: {0} GB" -f $free) $(if ($free -ge 40) {'Green'} else {'Yellow'})
if ($free -lt 40) {
    Fail "Not enough free space. The server plus its build needs about 40 GB; you have $free GB."
    return
}

# --- internet -----------------------------------------------------------
if (-not (Test-Connection github.com -Count 1 -Quiet -ErrorAction SilentlyContinue)) {
    Fail 'Cannot reach github.com. Check your internet connection and run this again.'
    return
}
Say '  Internet: reachable' Green

# =====================================================================
Head 2 8 'Where is your Star Wars Galaxies game folder?'

$BASE_TRES = @('bottom.tre','data_texture_00.tre','data_sample_00.tre','patch_00.tre')

function Test-ClientFolder($p) {
    if (-not $p -or -not (Test-Path $p)) { return $false }
    foreach ($t in $BASE_TRES) { if (-not (Test-Path (Join-Path $p $t))) { return $false } }
    return $true
}

$client = $state.client
if (-not (Test-ClientFolder $client)) {
    Say '  Looking for it...' Gray
    $guesses = @(
        'C:\SWGEmu','C:\Program Files\SWGEmu','C:\Program Files (x86)\SWGEmu',
        'C:\StarWarsGalaxies','C:\Program Files (x86)\StarWarsGalaxies',
        'D:\SWGEmu','D:\StarWarsGalaxies'
    ) + (Get-ChildItem 'C:\','D:\','E:\' -Directory -ErrorAction SilentlyContinue |
         Where-Object { $_.Name -match 'swg|galax|launcher' } | ForEach-Object { $_.FullName })

    $client = $guesses | Where-Object { Test-ClientFolder $_ } | Select-Object -First 1

    if ($client) {
        Say ''
        Say ("  Found game files here:") Green
        Say ("     $client") Green
        Say ''
        $ok = Read-Host '  Is that the right folder? (y/n)'
        if ($ok -ne 'y') { $client = $null }
    }

    while (-not (Test-ClientFolder $client)) {
        Say ''
        Say '  I need the folder containing your game files -' Gray
        Say '  the one with bottom.tre and data_texture_00.tre in it.' Gray
        Say ''
        Add-Type -AssemblyName System.Windows.Forms
        $dlg = New-Object System.Windows.Forms.FolderBrowserDialog
        $dlg.Description = 'Select your Star Wars Galaxies game folder'
        if ($dlg.ShowDialog() -ne 'OK') {
            Fail 'No folder chosen. Run this again when you know where your game files are.'
            Say '  If you do not have them yet: install any SWG emulator client first.' Yellow
            Say '  We cannot include the base game - it belongs to LucasArts.' Yellow
            return
        }
        $client = $dlg.SelectedPath
        if (-not (Test-ClientFolder $client)) {
            $missing = $BASE_TRES | Where-Object { -not (Test-Path (Join-Path $client $_)) }
            Say ''
            Say "  That folder is missing: $($missing -join ', ')" Red
            Say '  That does not look like a game folder. Try again.' Yellow
        }
    }
}
Say ("  Game files: $client") Green
$state.client = $client; Save-State $state

# =====================================================================
Head 3 8 'Where should the server go?'

$install = $state.install
if (-not $install) {
    Say '  Default: C:\SWGGenesis' Gray
    $a = Read-Host '  Press Enter to accept, or type a different folder'
    $install = if ([string]::IsNullOrWhiteSpace($a)) { 'C:\SWGGenesis' } else { $a.Trim() }
}
New-Item -ItemType Directory -Force -Path $install | Out-Null
Say ("  Server folder: $install") Green
$state.install = $install; Save-State $state

$wslInstall = '/mnt/' + $install.Substring(0,1).ToLower() + ($install.Substring(2) -replace '\\','/')
$wslClient  = '/mnt/' + $client.Substring(0,1).ToLower()  + ($client.Substring(2)  -replace '\\','/')

# =====================================================================
Head 4 8 'Installing Windows Subsystem for Linux'

$wslOk = $false
try { wsl.exe --status *>$null; $wslOk = ($LASTEXITCODE -eq 0) } catch {}

if (-not $wslOk) {
    Say '  Installing. Your computer will need to restart afterwards.' Gray
    & wsl.exe --install --no-launch 2>&1 | ForEach-Object { Say ('    ' + $_) DarkGray }
    Say ''
    Say '  ------------------------------------------------------------' Yellow
    Say '   RESTART YOUR COMPUTER NOW, then run this installer again.' Yellow
    Say '   It will carry on from here - nothing is lost.' Yellow
    Say '  ------------------------------------------------------------' Yellow
    Say ''
    $r = Read-Host '  Restart now? (y/n)'
    if ($r -eq 'y') { shutdown /r /t 5 }
    return
}
Say '  WSL: installed' Green
& wsl.exe --set-default-version 2 *>$null

$have = @()
try { $have = ((& wsl.exe --list --quiet) -join "`n") -replace "`0",'' -split "`n" |
        ForEach-Object { $_.Trim() } | Where-Object { $_ } } catch {}
if ($have -notcontains $DISTRO) {
    Say "  Installing Ubuntu (a few minutes)..." Gray
    & wsl.exe --install -d $DISTRO --no-launch 2>&1 | ForEach-Object { Say ('    ' + $_) DarkGray }
    Start-Sleep 5
}
& wsl.exe -d $DISTRO -u root -- /bin/true *>$null
if ($LASTEXITCODE -ne 0) { Fail "Could not start Ubuntu. Restart your computer and run this again."; return }
& wsl.exe -d $DISTRO -u root -- bash -c "printf '[boot]\nsystemd=true\n' > /etc/wsl.conf" *>$null
& wsl.exe --shutdown *>$null; Start-Sleep 8
& wsl.exe -d $DISTRO -u root -- /bin/true *>$null
Say '  Ubuntu: ready' Green

# =====================================================================
Head 5 8 'Installing Windows tools'

foreach ($pkg in @(
    @{id='Python.Python.3.12'; name='Python';      test={ Get-Command python -EA SilentlyContinue }},
    @{id='AutoHotkey.AutoHotkey'; name='AutoHotkey'; test={ Test-Path "$env:ProgramFiles\AutoHotkey" }}
)) {
    if (& $pkg.test) { Say ("  {0}: already installed" -f $pkg.name) Green; continue }
    Say ("  Installing {0}..." -f $pkg.name) Gray
    & winget.exe install --id $pkg.id --exact --silent `
        --accept-package-agreements --accept-source-agreements *>$null
    Say ("  {0}: done" -f $pkg.name) Green
}
$env:Path = [Environment]::GetEnvironmentVariable('Path','Machine') + ';' +
            [Environment]::GetEnvironmentVariable('Path','User')

# =====================================================================
Head 6 8 'Building the server'
Say '  This is the long part - roughly an hour. You can leave it running.' Gray
Say '  Progress appears below; nothing is frozen even when it is quiet.' Gray
Say ''

if (-not $state.dbpass) {
    $state.dbpass = -join ((48..57)+(65..90)+(97..122) | Get-Random -Count 24 | ForEach-Object {[char]$_})
    Save-State $state
}

$bash = @"
#!/usr/bin/env bash
set -uo pipefail
REPO="$wslInstall"
CLIENT="$wslClient"
DBPASS="$($state.dbpass)"
LOG="\$REPO/install_log.txt"
exec > >(tee -a "\$LOG") 2>&1
echo; echo "======== \$(date) ========"

step(){ echo; echo ">>> \$*"; }

export DEBIAN_FRONTEND=noninteractive
step "system packages"
apt-get update -y
apt-get install -y --no-install-recommends \
  build-essential cmake git ccache pkg-config default-jre \
  liblua5.3-dev libssl-dev libmariadb-dev libmariadb-dev-compat \
  libboost-dev libboost-thread-dev mariadb-server mariadb-client \
  screen gdb python3 curl ca-certificates || exit 1
apt-get install -y libdb5.3-dev 2>/dev/null || apt-get install -y libdb-dev

step "getting the server source"
git config --global --add safe.directory '*'
if [ -d "\$REPO/.git" ]; then
    git -C "\$REPO" pull --ff-only || echo "  (pull skipped)"
else
    git clone --recursive "$REPO_URL" "\$REPO" || exit 1
fi
git -C "\$REPO" submodule update --init --recursive

step "applying patches"
# The repo carries its own fixes for the upstream engine3 submodule.
# Running this every build is what stops a submodule update silently
# reintroducing a crash ten minutes into every boot.
if [ -f "\$REPO/patches/apply.sh" ]; then
    bash "\$REPO/patches/apply.sh" || echo "  !! patch step reported a problem"
else
    echo "  !! patches/apply.sh missing - the server may crash after ~10 minutes"
fi

step "database"
systemctl enable mariadb 2>/dev/null || true
systemctl start mariadb 2>/dev/null || service mariadb start
sleep 4
mysql -u root <<SQL
CREATE DATABASE IF NOT EXISTS genesis;
CREATE USER IF NOT EXISTS 'genesis'@'localhost' IDENTIFIED BY '\$DBPASS';
GRANT ALL PRIVILEGES ON genesis.* TO 'genesis'@'localhost';
FLUSH PRIVILEGES;
SQL
for f in swgemu.sql datatables.sql; do
    p="\$REPO/MMOCoreORB/sql/\$f"
    [ -f "\$p" ] || continue
    echo "  loading \$f"
    # These files hardcode 'USE swgemu' and fully qualify every table, so a
    # database named on the command line is silently ignored.
    sed -e '/CREATE DATABASE IF NOT EXISTS swgemu;/d' \
        -e 's/^USE swgemu;/USE genesis;/' \
        -e 's/\`swgemu\`\./\`genesis\`./g' "\$p" | mysql -u root genesis
done
mysql -u root genesis -e "UPDATE galaxy SET galaxy_id=1,name='Genesis',address='127.0.0.1',port=44463,pingport=44462 WHERE galaxy_id=2;"
mysql -u root genesis -e "INSERT IGNORE INTO galaxy (galaxy_id,name,address,port,pingport) VALUES (1,'Genesis','127.0.0.1',44463,44462);"

step "server configuration"
CONF="\$REPO/MMOCoreORB/bin/conf/config-local.lua"
mkdir -p "\$(dirname "\$CONF")"
cat > "\$CONF" <<CFG
-- Generated by the SWG Genesis installer. Safe to edit.
-- NOTE: genesis wraps everything in a Core3 table, so overrides must be
-- dotted. A bare "DBName = x" sets a global nothing reads.
Core3.DBHost = "127.0.0.1"
Core3.DBPort = 3306
Core3.DBName = "genesis"
Core3.DBUser = "genesis"
Core3.DBPass = "\$DBPASS"
Core3.MantisHost = ""

Core3.ORBPort        = 44419
Core3.LoginPort      = 44453
Core3.StatusPort     = 44455
Core3.PingPort       = 44462
Core3.ZoneServerPort = 44463
Core3.WebPorts       = 44460
Core3.MakeWeb        = 0
Core3.RESTServerPort = 0

Core3.ZoneGalaxyID = 1
Core3.AutoReg      = 1
Core3.CharacterBuilderEnabled = true

Core3.TrePath = "\$CLIENT"

-- Do NOT set Core3.ZonesEnabled. A zone subset segfaults the server:
-- ResourceSpawner::ghDumpAll() hardcodes ten planets and dereferences
-- an unloaded one on the first resource shift, minutes into boot.
CFG
echo "  wrote config-local.lua"

step "companion system content"
SRC="\$REPO/docs/companion_system/tools/companion_patch.tre"
if [ -f "\$SRC" ]; then
    cp -f "\$SRC" "\$CLIENT/companion_patch.tre" && echo "  installed companion_patch.tre into the game folder"
else
    echo "  !! companion_patch.tre not found in the repo"
fi

step "building (this is the hour)"
mkdir -p "\$REPO/MMOCoreORB/build/unix"
cd "\$REPO/MMOCoreORB/build/unix" || exit 1
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_IDL=ON -DCOMPILE_TESTS=OFF \
      -DENABLE_BUILD_CLIENT=OFF -DENABLE_ERROR_ON_WARNINGS=OFF \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -Wno-dev ../.. || exit 1
make -j"\$(nproc)" || exit 1

if [ -x "\$REPO/MMOCoreORB/bin/core3" ]; then
    ls -lh "\$REPO/MMOCoreORB/bin/core3"; echo "BUILD OK"; exit 0
fi
echo "!! core3 was not produced"; exit 1
"@

$sh = Join-Path $install '_install.sh'
[IO.File]::WriteAllText($sh, ($bash -replace "`r`n","`n"))
& wsl.exe -d $DISTRO -u root -- bash ($wslInstall + '/_install.sh') 2>&1 | ForEach-Object { Write-Host $_ }

if ($LASTEXITCODE -ne 0) {
    Fail 'The build did not finish.'
    Say "  The details are in $install\install_log.txt" Yellow
    Say '  Send that file to whoever gave you this installer.' Yellow
    return
}
Say ''
Say '  Server built' Green

# =====================================================================
Head 7 8 'Setting up networking'

foreach ($p in @(44453,44462,44463)) {
    netsh advfirewall firewall delete rule name="SWG Genesis UDP $p" *>$null
    netsh advfirewall firewall add rule name="SWG Genesis UDP $p" dir=in action=allow protocol=UDP localport=$p *>$null
}
Say '  Windows Firewall: opened for the game ports' Green

$relay = Join-Path $install 'server_relay'
if (Test-Path (Join-Path $relay 'relay.py')) {
    schtasks /Create /F /TN "SWG Genesis Server Relay" /SC ONSTART /RL HIGHEST /RU SYSTEM `
      /TR "cmd /c cd /d `"$relay`" && python relay.py >> relay-autostart.log 2>&1" *>$null
    Start-Process -FilePath 'python' -ArgumentList "`"$relay\relay.py`"" -WorkingDirectory $relay -WindowStyle Minimized
    Say '  Relay: running, and set to start with Windows' Green
} else {
    Say '  Relay: not found in the repo (skipped)' Yellow
}

# =====================================================================
Head 8 8 'Finishing up'

$ahk = Get-ChildItem "$env:ProgramFiles\AutoHotkey" -Filter 'AutoHotkey*64.exe' -Recurse -EA SilentlyContinue |
       Select-Object -First 1
$panel = Join-Path $install 'SWGGenesisControlPanel.ahk'
if ($ahk -and (Test-Path $panel)) {
    $lnk = Join-Path ([Environment]::GetFolderPath('Desktop')) 'SWG Genesis Control Panel.lnk'
    $sh2 = New-Object -ComObject WScript.Shell
    $s = $sh2.CreateShortcut($lnk)
    $s.TargetPath = $ahk.FullName; $s.Arguments = "`"$panel`""; $s.WorkingDirectory = $install
    $s.Save()
    Say '  Control panel: on your desktop' Green
    Start-Process -FilePath $ahk.FullName -ArgumentList "`"$panel`"" -WorkingDirectory $install
}

Say ''
Say '  ================================================================' Green
Say '     DONE  -  your server is installed' Green
Say '  ================================================================' Green
Say ''
Say '  In the control panel window, press START.' Cyan
Say '  The first start takes 15-25 minutes while it builds the galaxy.' Gray
Say '  When it says the server is up, press PLAY.' Cyan
Say ''
Say '  To let other people join, forward these ports on your router' Gray
Say '  to this computer:' Gray
Say ''
Say '      44453   UDP        44462   UDP        44463   UDP' Yellow
Say ''
Say '  All three are UDP. Do not forward any others.' Gray
Say ''

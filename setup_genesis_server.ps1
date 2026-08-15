# =====================================================================
#  SWGGenesis — full server setup on a fresh Windows install
#  Written by SWGReturn (Claude), 2026-08-15.
#
#  Run it as often as you like. Every stage checks whether it is already
#  done and skips itself, so if it stops halfway (or needs a reboot) you
#  just launch it again and it picks up where it left off.
#
#  What it does, in order:
#    1. Copy D:\SWGGenesis  ->  C:\SWGGenesis   (skips build output)
#    2. Install WSL2 + Ubuntu 24.04
#    3. Inside Ubuntu: install the whole Core3 toolchain + MariaDB
#    4. Create the 'genesis' database and load the schema
#    5. Build core3
#
#  Everything is logged to C:\SWGGenesis\setup_log.txt
# =====================================================================

$ErrorActionPreference = 'Continue'

$Src    = 'D:\SWGGenesis'
$Dst    = 'C:\SWGGenesis'
$Distro = 'Ubuntu-24.04'
$Log    = 'C:\SWGGenesis\setup_log.txt'

New-Item -ItemType Directory -Force -Path $Dst | Out-Null

function Say($m, $c = 'Gray') {
    Write-Host $m -ForegroundColor $c
    try { Add-Content -Path $Log -Value ("[{0}] {1}" -f (Get-Date -Format 'HH:mm:ss'), $m) -ErrorAction SilentlyContinue } catch {}
}
function Head($m) { Say ''; Say ('  == ' + $m + ' ' + ('=' * [Math]::Max(0, 56 - $m.Length))) Cyan }

Clear-Host
Say ''
Say '  ############################################################' Cyan
Say '  #   SWGGenesis  --  full server setup                      #' Cyan
Say '  ############################################################' Cyan
Say ("  started {0}" -f (Get-Date))

# --- must be admin ---------------------------------------------------
$isAdmin = ([Security.Principal.WindowsPrincipal] `
            [Security.Principal.WindowsIdentity]::GetCurrent()
           ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Say ''
    Say '  This needs to run as Administrator (it installs Windows features).' Red
    Say '  Close this, RIGHT-CLICK "SETUP EVERYTHING.cmd", and choose' Yellow
    Say '  "Run as administrator".' Yellow
    Say ''
    return
}

# =====================================================================
# STAGE 1 — get the code onto the SSD
# =====================================================================
Head 'STAGE 1  copy the repo to the C: SSD'

if (Test-Path (Join-Path $Dst 'MMOCoreORB\src')) {
    Say '  Already copied — C:\SWGGenesis has the source tree. Skipping.' Green
}
elseif (-not (Test-Path $Src)) {
    Say '  D:\SWGGenesis not found and C: has no source tree.' Red
    Say '  Nothing to copy from. Stopping.' Red
    return
}
else {
    $skipDirs  = @('build', 'databases', '.git-rewrite', '__pycache__', '_idl_scratch_to_delete')
    # The last three are deliberately excluded: Claude has already placed
    # corrected copies on C: with the paths repointed from D: to C: and the
    # WSL distro changed from Debian to Ubuntu-24.04. Copying the D: originals
    # over them would undo that.
    $skipFiles = @('core3', 'core3client', 'testsuite3', 'console.log', 'console.log.prev',
                   '*.bak', '*.bak-*',
                   'swggenesis_menu.py', 'SWGGenesisControlPanel.ahk', 'run_backup.cmd')

    $freeGB = [math]::Round((Get-PSDrive C).Free / 1GB, 2)
    Say ("  Free on C: {0:N2} GB" -f $freeGB)
    if ($freeGB -lt 30) {
        Say '  WARNING: under 30 GB free. The copy plus a build tree needs more' Yellow
        Say '  than that. Continuing anyway, but it may run out of room.' Yellow
    }

    Say '  Copying (reading from the spinning disk — this is the slow part)...'
    $rcArgs = @($Src, $Dst, '/E', '/R:2', '/W:5', '/NP', '/NFL', '/NDL', "/LOG+:$Dst\copy_log.txt")
    foreach ($d in $skipDirs)  { $rcArgs += '/XD'; $rcArgs += $d }
    foreach ($f in $skipFiles) { $rcArgs += '/XF'; $rcArgs += $f }

    & robocopy.exe @rcArgs | Out-Null
    $rc = $LASTEXITCODE

    if ($rc -ge 8) { Say ("  Copy FAILED (robocopy code {0}). See copy_log.txt" -f $rc) Red; return }
    Say ("  Copy OK (robocopy code {0})." -f $rc) Green
}

# =====================================================================
# STAGE 2 — WSL2
# =====================================================================
Head 'STAGE 2  WSL2'

$wslOk = $false
try { wsl.exe --status 2>&1 | Out-Null; $wslOk = ($LASTEXITCODE -eq 0) } catch { $wslOk = $false }

if (-not $wslOk) {
    Say '  WSL is not installed. Installing it now...'
    Say '  (this enables two Windows features and may want a reboot)'
    & wsl.exe --install --no-launch 2>&1 | ForEach-Object { Say ('    ' + $_) }

    Say ''
    Say '  ------------------------------------------------------------' Yellow
    Say '   REBOOT NOW, then run this same file again.' Yellow
    Say '   It will skip everything already done and carry on.' Yellow
    Say '  ------------------------------------------------------------' Yellow
    Say ''
    return
}
Say '  WSL is present.' Green

& wsl.exe --set-default-version 2 2>&1 | Out-Null

# --- the distro ------------------------------------------------------
$installed = @()
try {
    $raw = & wsl.exe --list --quiet 2>&1
    $installed = ($raw -join "`n") -replace "`0", '' -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ }
} catch {}

if ($installed -notcontains $Distro) {
    Say ("  Installing {0} ..." -f $Distro)
    & wsl.exe --install -d $Distro --no-launch 2>&1 | ForEach-Object { Say ('    ' + $_) }
    Start-Sleep -Seconds 5
}

# First run initialises the distro. --no-launch above avoided the
# interactive "create a username" prompt; running as root keeps it that way.
Say '  Initialising the distro...'
& wsl.exe -d $Distro -u root -- /bin/true 2>&1 | ForEach-Object { Say ('    ' + $_) }

if ($LASTEXITCODE -ne 0) {
    Say ("  Could not start {0}." -f $Distro) Red
    Say '  If Windows asked for a reboot earlier, reboot and run this again.' Yellow
    return
}
Say ("  {0} is running." -f $Distro) Green

# --- systemd, so 'systemctl' and MariaDB behave normally -------------
& wsl.exe -d $Distro -u root -- bash -c "printf '[boot]\nsystemd=true\n' > /etc/wsl.conf" 2>&1 | Out-Null
Say '  Enabled systemd inside the distro.'

& wsl.exe --shutdown 2>&1 | Out-Null
Start-Sleep -Seconds 8
& wsl.exe -d $Distro -u root -- /bin/true 2>&1 | Out-Null

# =====================================================================
# STAGE 3 — everything Linux-side
# =====================================================================
Head 'STAGE 3  toolchain, database, build'

# Written with LF line endings — CRLF would break the shebang.
$bash = @'
#!/usr/bin/env bash
set -uo pipefail

REPO=/mnt/c/SWGGenesis
DB=genesis
DBUSER=returns
DBPASS=ReturnsDB2026
LOG=$REPO/setup_wsl_log.txt

exec > >(tee -a "$LOG") 2>&1
echo
echo "================ genesis WSL setup  $(date) ================"

step() { echo; echo "-------- $* --------"; }
fail() { echo "!! FAILED: $*"; }

if [ ! -d "$REPO/MMOCoreORB/src" ]; then
    fail "no source tree at $REPO/MMOCoreORB/src"
    exit 1
fi

export DEBIAN_FRONTEND=noninteractive

step "apt update"
apt-get update -y || fail "apt update"

step "toolchain + libraries"
# Package list taken from this repo's own MMOCoreORB/docker/Dockerfile,
# plus what is needed to actually run the server (screen, gdb, mariadb).
PKGS="build-essential cmake git ccache pkg-config default-jre \
liblua5.3-dev libssl-dev libmariadb-dev libmariadb-dev-compat \
libboost-dev libboost-thread-dev \
mariadb-server mariadb-client screen gdb python3 python3-pip \
curl ca-certificates lsb-release"

apt-get install -y --no-install-recommends $PKGS || fail "apt install (main set)"

# Berkeley DB: the Dockerfile pins libdb5.3-dev, but the name moves
# between releases. Take whichever one this Ubuntu actually has.
if ! apt-get install -y libdb5.3-dev 2>/dev/null; then
    echo "libdb5.3-dev unavailable, falling back to libdb-dev"
    apt-get install -y libdb-dev || fail "libdb-dev"
fi

step "git safe.directory (repo lives on a Windows mount)"
git config --global --add safe.directory '*' || true

step "repoint tooling paths from D: to C:"
# swggenesis_menu.py, the AHK control panel and run_backup.cmd all have
# the repo path hardcoded ~30 times as /mnt/d/SWGGenesis and D:\SWGGenesis.
# Rewrite ONLY the SWGGenesis ones -- D:\Launcher\newreturnbenserver is the
# client content and has NOT moved, so it must stay pointing at D:.
for f in swggenesis_menu.py SWGGenesisControlPanel.ahk run_backup.cmd; do
    p="$REPO/$f"
    if [ -f "$p" ]; then
        cp -n "$p" "$p.bak-dtoc" 2>/dev/null || true
        before=$(grep -c -e '/mnt/d/SWGGenesis' -e 'D:\\SWGGenesis' "$p" 2>/dev/null || echo 0)
        sed -i -e 's#/mnt/d/SWGGenesis#/mnt/c/SWGGenesis#g' \
               -e 's#D:\\SWGGenesis#C:\\SWGGenesis#g' "$p"
        after=$(grep -c -e '/mnt/d/SWGGenesis' -e 'D:\\SWGGenesis' "$p" 2>/dev/null || echo 0)
        echo "  $f: rewrote $before reference(s), $after left"
    else
        echo "  $f not present, skipping"
    fi
done
echo "  (D:\\Launcher\\newreturnbenserver deliberately left alone -- client content did not move)"

step "engine3 submodule"
cd "$REPO" || exit 1
git submodule update --init --recursive || fail "submodule update"

step "start MariaDB"
if command -v systemctl >/dev/null 2>&1 && systemctl list-units >/dev/null 2>&1; then
    systemctl enable mariadb  || true
    systemctl start  mariadb  || service mariadb start || fail "mariadb start"
else
    service mariadb start || fail "mariadb start"
fi
sleep 4
mysqladmin status >/dev/null 2>&1 && echo "MariaDB is up" || fail "MariaDB did not come up"

step "create database + user"
mysql -u root <<SQL
CREATE DATABASE IF NOT EXISTS \`$DB\`;
CREATE USER IF NOT EXISTS '$DBUSER'@'localhost' IDENTIFIED BY '$DBPASS';
CREATE USER IF NOT EXISTS '$DBUSER'@'127.0.0.1' IDENTIFIED BY '$DBPASS';
GRANT ALL PRIVILEGES ON \`$DB\`.* TO '$DBUSER'@'localhost';
GRANT ALL PRIVILEGES ON \`$DB\`.* TO '$DBUSER'@'127.0.0.1';
FLUSH PRIVILEGES;
SQL
[ $? -eq 0 ] && echo "database '$DB' and user '$DBUSER' ready" || fail "db/user creation"

step "load schema"
# swgemu.sql hardcodes 'CREATE DATABASE swgemu / USE swgemu' and fully
# qualifies every table as `swgemu`.`x`, so passing a database on the
# command line is IGNORED. Rewrite the name in the stream instead.
for f in swgemu.sql datatables.sql; do
    p="$REPO/MMOCoreORB/sql/$f"
    if [ -f "$p" ]; then
        echo "  loading $f"
        sed -e '/CREATE DATABASE IF NOT EXISTS swgemu;/d' \
            -e "s/^USE swgemu;/USE $DB;/" \
            -e "s/\`swgemu\`\./\`$DB\`./g" "$p" | mysql -u root "$DB" || fail "load $f"
    else
        echo "  $f not found, skipping"
    fi
done
echo "  tables now: $(mysql -u root -N -B -e "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='$DB';")"

step "galaxy row (ZoneGalaxyID = 3)"
# ZoneServer::initialize does SELECT name FROM galaxy WHERE galaxy_id=<ZoneGalaxyID>
# and calls Logger::fatal() -> SIGABRT when there is no row.
mysql -u root "$DB" -e \
  "UPDATE galaxy SET galaxy_id=3, name='Genesis', address='127.0.0.1', port=46463, pingport=46462 WHERE galaxy_id=2;" || true
mysql -u root "$DB" -e \
  "INSERT IGNORE INTO galaxy (galaxy_id,name,address,port,pingport) VALUES (3,'Genesis','127.0.0.1',46463,46462);" || true
mysql -u root "$DB" -e "SELECT galaxy_id,name,address,port,pingport FROM galaxy;" || true

step "un-subset the zones"
# A zone subset segfaults genesis: ResourceSpawner::ghDumpAll()
# (ResourceSpawner.cpp:414) hardcodes ten planets and dereferences
# getZoneResourceList() with no null check, so any planet in that list
# which is not loaded crashes the first resource shift -- minutes into
# boot, long after everything looks healthy. Run the full default set.
CFG="$REPO/MMOCoreORB/bin/conf/config-local.lua"
if [ -f "$CFG" ]; then
    python3 - "$CFG" <<'PY'
import re, sys, shutil
p = sys.argv[1]
s = open(p, encoding='utf-8', errors='replace').read()
m = re.search(r'^Core3\.ZonesEnabled\s*=\s*\{.*?\}', s, re.S | re.M)
if m:
    shutil.copyfile(p, p + '.bak-zonesfix')
    block = m.group(0)
    commented = '\n'.join('-- ' + ln for ln in block.split('\n'))
    s = s[:m.start()] + \
        '-- DISABLED by setup 2026-08-15: a zone subset segfaults genesis in\n' \
        '-- ResourceSpawner::ghDumpAll(). Falling through to config.lua\'s\n' \
        '-- full default planet set instead.\n' + commented + s[m.end():]
    open(p, 'w', encoding='utf-8').write(s)
    print("  ZonesEnabled override commented out (backup: config-local.lua.bak-zonesfix)")
else:
    print("  no ZonesEnabled override found - already using the full set")
PY
else
    echo "  config-local.lua not found at $CFG"
fi

step "check the client content path (TrePath)"
TRE=$(grep -oP 'Core3\.TrePath\s*=\s*"\K[^"]+' "$CFG" 2>/dev/null | head -1)
if [ -n "$TRE" ]; then
    if [ -d "$TRE" ]; then
        echo "  OK: $TRE  ($(ls -1 "$TRE"/*.tre 2>/dev/null | wc -l) .tre files)"
    else
        fail "TrePath $TRE does not exist -- the server cannot boot without it"
    fi
fi

step "configure + build core3  (this is the long one)"
# COMPILE_TESTS=OFF is mandatory: CMakeLists.txt adds
# utils/googletest-release-1.10.0, which their .gitignore ('*test*')
# excludes from the repo, so a fresh clone cannot configure without it.
mkdir -p "$REPO/MMOCoreORB/build/unix"
cd "$REPO/MMOCoreORB/build/unix" || exit 1
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DBUILD_IDL=ON \
      -DCOMPILE_TESTS=OFF \
      -DENABLE_ERROR_ON_WARNINGS=OFF \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
      -Wno-dev ../.. || { fail "cmake configure"; exit 1; }

NPROC=$(nproc)
echo "  building with -j$NPROC"
make -j"$NPROC" || { fail "make"; exit 1; }

step "result"
if [ -x "$REPO/MMOCoreORB/bin/core3" ]; then
    ls -lh "$REPO/MMOCoreORB/bin/core3"
    echo
    echo "BUILD OK -- core3 is ready."
else
    fail "core3 binary not found at $REPO/MMOCoreORB/bin/core3"
fi

echo
echo "================ setup finished  $(date) ================"
'@

$bashPath = Join-Path $Dst 'setup_inside_wsl.sh'
[IO.File]::WriteAllText($bashPath, ($bash -replace "`r`n", "`n"))
Say ('  Wrote ' + $bashPath)

Say '  Handing over to Ubuntu. Package install + build takes a while —'
Say '  the build alone is typically 40-90 minutes. Leave it running.'
Say ''

& wsl.exe -d $Distro -u root -- bash /mnt/c/SWGGenesis/setup_inside_wsl.sh 2>&1 |
    ForEach-Object { Write-Host $_ }

$wslCode = $LASTEXITCODE

# =====================================================================
Head 'DONE'
if ($wslCode -eq 0 -and (Test-Path (Join-Path $Dst 'MMOCoreORB\bin\core3'))) {
    Say '  core3 built successfully on the C: SSD.' Green
} else {
    Say ("  Setup stopped early (exit {0})." -f $wslCode) Yellow
    Say '  The detail is in C:\SWGGenesis\setup_wsl_log.txt' Yellow
}
Say ''
Say '  Send Claude these two files and it will take it from there:' Cyan
Say '    C:\SWGGenesis\setup_log.txt' Cyan
Say '    C:\SWGGenesis\setup_wsl_log.txt' Cyan
Say ''

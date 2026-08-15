# =====================================================================
#  Finish the genesis build
#  Written by SWGReturn (Claude), 2026-08-15.
#
#  WHY THIS EXISTS
#  The first build aborts. Not because of anything in your setup --
#  `core3client`, a bench/debug client that ships in this repo, does not
#  compile against the pinned engine3:
#
#     src/client/ClientCore.h:12:31: error: expected class-name before ',' token
#         class ClientCore : public Core, public Logger
#
#  ClientCore.h only includes system/lang.h, and `Core` is no longer
#  declared there, so ~60 cascading errors follow. It is irrelevant to
#  running the server -- but it is part of the default `all` target, so
#  make aborts before core3 ever links.
#
#  Upstream ships a switch for exactly this: ENABLE_BUILD_CLIENT.
#  Turning it off means zero edits to their source.
#
#  Every object file the first run compiled is reused, so this picks up
#  roughly where it stopped rather than starting over.
# =====================================================================

$ErrorActionPreference = 'Continue'

$Distro = 'Ubuntu-24.04'
$Repo   = 'C:\SWGGenesis'
$Bin    = 'C:\SWGGenesis\MMOCoreORB\bin\core3'

function Say($m, $c = 'Gray') { Write-Host $m -ForegroundColor $c }

Clear-Host
Say ''
Say '  ############################################################' Cyan
Say '  #   Finish the genesis build                               #' Cyan
Say '  ############################################################' Cyan
Say ''

if (Test-Path $Bin) {
    $sz = [math]::Round((Get-Item $Bin).Length / 1MB, 1)
    Say ("  core3 already exists ({0} MB, built {1})." -f $sz, (Get-Item $Bin).LastWriteTime) Yellow
    Say '  Re-running anyway will just rebuild anything out of date.' Gray
    Say ''
    $go = Read-Host '  Continue? (y/n)'
    if ($go -ne 'y') { return }
}

$bash = @'
#!/usr/bin/env bash
set -uo pipefail

REPO=/mnt/c/SWGGenesis
BUILD=$REPO/MMOCoreORB/build/unix
LOG=$REPO/finish_build_log.txt

exec > >(tee -a "$LOG") 2>&1
echo
echo "============ finish build  $(date) ============"

cd "$BUILD" || { echo "!! no build dir at $BUILD"; exit 1; }

echo
echo "-------- reconfigure with core3client disabled --------"
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DBUILD_IDL=ON \
      -DCOMPILE_TESTS=OFF \
      -DENABLE_ERROR_ON_WARNINGS=OFF \
      -DENABLE_BUILD_CLIENT=OFF \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
      -Wno-dev ../.. || { echo "!! cmake reconfigure FAILED"; exit 1; }

echo
echo "-------- build (reusing everything already compiled) --------"
NPROC=$(nproc)
echo "  make -j$NPROC"
make -j"$NPROC"
RC=$?

echo
echo "-------- result --------"
if [ -x "$REPO/MMOCoreORB/bin/core3" ]; then
    ls -lh "$REPO/MMOCoreORB/bin/core3"
    echo
    echo "BUILD OK -- core3 is ready."
    exit 0
else
    echo "!! core3 still not built (make exit $RC)"
    echo "   Last errors:"
    grep -E "error:" "$LOG" | tail -20
    exit 1
fi
'@

$path = Join-Path $Repo 'finish_build.sh'
[IO.File]::WriteAllText($path, ($bash -replace "`r`n", "`n"))

Say '  Handing over to Ubuntu. This resumes from the cached objects,' Gray
Say '  so it is much shorter than the first run.' Gray
Say ''

& wsl.exe -d $Distro -u root -- bash /mnt/c/SWGGenesis/finish_build.sh 2>&1 |
    ForEach-Object { Write-Host $_ }

$code = $LASTEXITCODE

Say ''
Say '  ============================================================' Cyan
if ($code -eq 0 -and (Test-Path $Bin)) {
    $sz = [math]::Round((Get-Item $Bin).Length / 1MB, 1)
    Say ("   BUILD OK -- core3 is {0} MB" -f $sz) Green
    Say ''
    Say '   Next: run "SETUP CLIENT.cmd" if you have not yet, then open' Gray
    Say '   the control panel and press Start.' Gray
} else {
    Say '   Still failing. Send Claude:' Yellow
    Say '     C:\SWGGenesis\finish_build_log.txt' Yellow
}
Say '  ============================================================' Cyan
Say ''

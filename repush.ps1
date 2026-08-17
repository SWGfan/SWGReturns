# =====================================================================
#  Reset to what GitHub actually has, then push cleanly
#  Written by SWGReturn (Claude), 2026-08-15.
#
#  A push was rejected again for MMOCoreORB/bin/databases -- the runtime
#  object DB, 628 MB against GitHub's 100 MB limit. Rather than guess how
#  it got re-staged, this resets the local branch to origin/main (keeping
#  every working-tree change), re-applies the ignore rules, stages
#  deliberately, and pushes.
#
#  `git reset --soft origin/main` is safe: it moves the branch pointer
#  only. No file on disk is touched, nothing is lost.
#
#  It also removes the old commit script, which used `git add -A` and is
#  what caused this in the first place.
# =====================================================================

$ErrorActionPreference = 'Continue'
$Distro = 'Ubuntu-24.04'
function Say($m, $c = 'Gray') { Write-Host $m -ForegroundColor $c }

Clear-Host
Say ''
Say '  ############################################################' Cyan
Say '  #   Reset to GitHub and push cleanly                       #' Cyan
Say '  ############################################################' Cyan
Say ''

# Retire the script that caused this - it used `git add -A`.
foreach ($f in @('C:\SWGGenesis\COMMIT AND PUSH.cmd','C:\SWGGenesis\commit_and_push.ps1')) {
    if (Test-Path $f) { Remove-Item $f -Force -EA SilentlyContinue; Say ("  removed $f") DarkGray }
}

$prep = @'
#!/usr/bin/env bash
set -uo pipefail
cd /mnt/c/SWGGenesis || exit 1

echo "======== where are we ========"
echo "  local commits:"
git log --oneline -5 | sed 's/^/    /'
echo
git fetch origin 2>&1 | sed 's/^/  /'
echo "  remote main:"
git log --oneline -3 origin/main 2>/dev/null | sed 's/^/    /' || echo "    (could not read origin/main)"

echo
echo "======== resetting to origin/main ========"
if git rev-parse --verify origin/main >/dev/null 2>&1; then
    git reset --soft origin/main && echo "  branch pointer moved to origin/main (no files touched)"
    git reset >/dev/null 2>&1 && echo "  unstaged everything"
else
    echo "  !! no origin/main - leaving history alone"
    git reset >/dev/null 2>&1
fi
echo "  now at:"
git log --oneline -1 | sed 's/^/    /'

echo
echo "======== ignore rules ========"
if ! grep -q "SWGGENESIS SETUP 2026-08-15" .gitignore; then
cat >> .gitignore <<'IGN'

# ===== SWGGENESIS SETUP 2026-08-15 =====
# Runtime object database. __db.001 alone is 628 MB against GitHub's
# 100 MB hard limit - this is what got pushes rejected twice.
MMOCoreORB/bin/databases/

setup_log.txt
setup_wsl_log.txt
finish_build_log.txt
fix_rebuild_log.txt
install_log.txt
copy_log.txt
client_setup_log.txt
relay-autostart.log
server_relay/relay.log
backups/
content_bundle/
panel_settings.ini
.genesis_timings
.genesis_timing_marks
_commit_prep.sh
_commit_do.sh
_fix_prep.sh
_fix_do.sh
_install.sh

# The broad `*.sh` rule above swallows our own patch tooling.
# patches/apply.sh re-applies the engine3 fixes after a submodule
# update; without it a fresh clone crashes ~10 minutes into every boot.
!patches/apply.sh
IGN
echo "  appended"
else
    echo "  already present"
fi
git rm -r --cached --quiet MMOCoreORB/bin/databases >/dev/null 2>&1 && echo "  untracked bin/databases" || true

echo
echo "======== staging deliberately ========"
git add -u
git add .gitignore
git add -f patches/apply.sh patches/engine3-time-overflow.patch 2>/dev/null
git add swggenesis_menu.py SWGGenesisControlPanel.ahk run_backup.cmd 2>/dev/null
git add server_relay/relay.py "server_relay/START RELAY.cmd" "server_relay/INSTALL RELAY AUTOSTART.cmd" 2>/dev/null
git add ServerInstaller/ 2>/dev/null
git add ./*.ps1 ./*.cmd 2>/dev/null

echo "  staged: $(git diff --cached --name-only | wc -l) files"
echo
echo "  --- anything over 20 MB? ---"
over=0
git diff --cached --name-only -z | while IFS= read -r -d '' f; do
    [ -f "$f" ] || continue
    sz=$(stat -c%s "$f" 2>/dev/null || echo 0)
    [ "$sz" -gt 20000000 ] && echo "    !! $f ($((sz/1000000)) MB)"
done
echo "  (nothing above = safe)"
echo
echo "  --- patches present? ---"
git diff --cached --name-only | grep -E "^patches/" | sed 's/^/    /' || \
  git ls-files patches/ | sed 's/^/    already committed: /' || echo "    !! patches/ MISSING"
'@

[IO.File]::WriteAllText('C:\SWGGenesis\_repush_prep.sh', ($prep -replace "`r`n","`n"))
& wsl.exe -d $Distro -u root -- bash /mnt/c/SWGGenesis/_repush_prep.sh 2>&1 | ForEach-Object { Write-Host $_ }

Say ''
Say '  ============================================================' Yellow
Say '   Nothing pushed yet. Check the size list above is empty.' Yellow
Say '  ============================================================' Yellow
Say ''
if ((Read-Host '  Type YES to commit and push') -ne 'YES') { Say ''; Say '  Cancelled.' Yellow; Say ''; return }

$doit = @'
#!/usr/bin/env bash
set -uo pipefail
cd /mnt/c/SWGGenesis || exit 1
if git diff --cached --quiet; then
    echo "nothing new to commit - checking whether we are already up to date"
else
    git commit -m "Installer, content bundler and relay updates

ServerInstaller/ - host installer, content bundler and game-file
transfer helper for SWGfan/ServerInstaller. Self-elevating, checks BIOS
virtualisation before anything else, clones the server repo and calls
its patches/apply.sh rather than hardcoding fixes, generates its own
config-local.lua (bin/conf is gitignored so a clone brings none).

server_relay - reads ports from config-local.lua instead of the
hardcoded 44xxx block. A relay on the wrong ports forwards nothing and
reports nothing; UDP login just hangs.

gitignore - MMOCoreORB/bin/databases/ and the installer logs. The
object DB is 628 MB and got two pushes rejected."
fi
echo
echo "======== pushing ========"
git push origin HEAD
rc=$?
echo
[ $rc -eq 0 ] && { echo "PUSH OK"; git log --oneline -1; } || echo "!! push failed ($rc)"
exit $rc
'@

[IO.File]::WriteAllText('C:\SWGGenesis\_repush_do.sh', ($doit -replace "`r`n","`n"))
& wsl.exe -d $Distro -u root -- bash /mnt/c/SWGGenesis/_repush_do.sh 2>&1 | ForEach-Object { Write-Host $_ }

Say ''
if ($LASTEXITCODE -eq 0) { Say '   Pushed.' Green } else { Say '   Push did not complete.' Yellow }
Say ''

# =====================================================================
#  Commit and push tonight's work to SWGfan/SWGReturns
#  Written by SWGReturn (Claude), 2026-08-15.
#
#  Runs in two halves so you see everything before anything is written:
#    PART 1  turns the engine3 fix into a patch file, then shows you
#            exactly what would be committed.
#    PART 2  only runs after you type YES.
# =====================================================================

$ErrorActionPreference = 'Continue'
$Distro = 'Ubuntu-24.04'

function Say($m, $c = 'Gray') { Write-Host $m -ForegroundColor $c }

Clear-Host
Say ''
Say '  ############################################################' Cyan
Say '  #   Commit and push                                        #' Cyan
Say '  ############################################################' Cyan
Say ''

# ---------------------------------------------------------------- PART 1
$prep = @'
#!/usr/bin/env bash
set -uo pipefail
REPO=/mnt/c/SWGGenesis
ENGINE3=$REPO/MMOCoreORB/utils/engine3
PATCHES=$REPO/patches

cd "$REPO" || exit 1

echo "======== 1. engine3 fix -> patch file ========"
mkdir -p "$PATCHES"

if git -C "$ENGINE3" diff --quiet 2>/dev/null; then
    if [ -f "$PATCHES/engine3-time-overflow.patch" ]; then
        echo "  submodule is clean and the patch file already exists - nothing to do"
    else
        echo "  !! submodule is clean but there is no patch file."
        echo "     The Time.h fix may have been lost. Check Time.h:252 says 'len -= ret;'"
    fi
else
    git -C "$ENGINE3" diff > "$PATCHES/engine3-time-overflow.patch"
    echo "  wrote patches/engine3-time-overflow.patch ($(wc -l < "$PATCHES/engine3-time-overflow.patch") lines)"
    echo
    sed -n '1,40p' "$PATCHES/engine3-time-overflow.patch"
fi

echo
echo "======== 2. patch applier (runs before every build) ========"
cat > "$PATCHES/apply.sh" <<'APPLY'
#!/usr/bin/env bash
# Re-applies our engine3 fixes after any `git submodule update`.
#
# engine3 is an upstream submodule, so anything we change there is wiped
# whenever it is updated. Carrying the fixes as patch files and applying
# them on every build turns a silent time-bomb into a routine step.
#
# Idempotent: a patch already applied is detected and skipped.
set -uo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENGINE3="$REPO/MMOCoreORB/utils/engine3"
rc=0
shopt -s nullglob
for p in "$REPO"/patches/*.patch; do
    name="$(basename "$p")"
    if git -C "$ENGINE3" apply --check --reverse "$p" >/dev/null 2>&1; then
        echo "  already applied: $name"
    elif git -C "$ENGINE3" apply "$p" >/dev/null 2>&1; then
        echo "  APPLIED: $name"
    else
        echo "  !! FAILED to apply: $name  (upstream may have changed this code)"
        rc=1
    fi
done
exit $rc
APPLY
chmod +x "$PATCHES/apply.sh"
echo "  wrote patches/apply.sh"
echo
echo "  testing it now:"
bash "$PATCHES/apply.sh"

echo
echo "======== 3. what would be committed ========"
git status --short
echo
echo "-------- summary of changes --------"
git diff --stat
echo
echo "-------- untracked files --------"
git ls-files --others --exclude-standard | head -40
echo
echo "-------- push target --------"
git remote get-url origin | sed -E 's#//[^@]*@#//***@#'
git branch --show-current
'@

$p1 = 'C:\SWGGenesis\_commit_prep.sh'
[IO.File]::WriteAllText($p1, ($prep -replace "`r`n", "`n"))
& wsl.exe -d $Distro -u root -- bash /mnt/c/SWGGenesis/_commit_prep.sh 2>&1 | ForEach-Object { Write-Host $_ }

Say ''
Say '  ============================================================' Yellow
Say '   Read the above. Nothing has been committed or pushed yet.' Yellow
Say '  ============================================================' Yellow
Say ''
$ans = Read-Host '  Type YES to commit and push'
if ($ans -ne 'YES') { Say ''; Say '  Cancelled. Nothing was committed.' Yellow; Say ''; return }

# ---------------------------------------------------------------- PART 2
$doit = @'
#!/usr/bin/env bash
set -uo pipefail
REPO=/mnt/c/SWGGenesis
cd "$REPO" || exit 1

git config user.name  >/dev/null 2>&1 || git config user.name  "SWGfan"
git config user.email >/dev/null 2>&1 || git config user.email "nicholaswill86@gmail.com"

echo "======== staging ========"
git add -A
git status --short | head -40

echo
echo "======== committing ========"
git commit -F - <<'MSG'
Rebuild on a fresh Windows install; fix a real buffer overflow in engine3

Machine was reinstalled from bare Windows: no WSL, no toolchain, no MySQL.
Rebuilt end to end and verified in-game.

engine3 buffer overflow (the important one)
  system/lang/Time.h getFormattedTimeFull() computed the space left in a
  128-byte buffer as `len -= ret - 1`, one byte more than actually
  remains at &buf[ret]. snprintf was therefore told 110 bytes when 109
  were free. Ubuntu 24.04 defaults to -D_FORTIFY_SOURCE=3, glibc detected
  the overrun and called abort(); the server died ~643s into every boot
  inside OnlinePlayerLogTask -> logOnlinePlayers -> getFormattedTimeFull.
  Debian does not fortify by default, so every previous build wrote past
  that buffer silently. Fix is one character: `len -= ret;`

  Carried as patches/engine3-time-overflow.patch rather than an edit in
  the submodule, since `git submodule update` would silently revert it
  and reintroduce a crash with no obvious cause. patches/apply.sh
  re-applies all patches idempotently and should run before every build.

build
  -DENABLE_BUILD_CLIENT=OFF added to the build flags. core3client does
  not compile against the pinned engine3 (ClientCore.h inherits Core,
  which system/lang.h no longer declares) and, being part of the default
  `all` target, its failure aborts make before core3 links. Upstream
  ships this switch, so source divergence stays at zero.

tooling
  Repo moved off the spinning HDD to the C: SSD; ~39 hardcoded D: paths
  repointed across swggenesis_menu.py, SWGGenesisControlPanel.ahk and
  run_backup.cmd. D:\Launcher references deliberately left alone.
  WSL distro Debian -> Ubuntu-24.04, which is named explicitly in the
  panel and in run_backup.cmd; missing that would have silently broken
  every panel button and the nightly scheduled backup.
  Windows username no longer hardcoded (A_UserName) - the reinstall
  changed it and every panel action would have failed on a dead path.
  Client folder is now C:\SWGEmu rather than the launcher install.

config
  Core3.ZonesEnabled override removed. A zone subset segfaults genesis in
  ResourceSpawner::ghDumpAll(), which hardcodes ten planets and
  dereferences getZoneResourceList() with no null check.

server_relay
  UDP relay now reads its ports from config-local.lua instead of the
  hardcoded 44xxx block, and names the WSL distro explicitly. A relay on
  the wrong ports forwards nothing and reports nothing - UDP login just
  hangs.
MSG

echo
echo "======== pushing ========"
git push origin HEAD
rc=$?
echo
if [ $rc -eq 0 ]; then
    echo "PUSH OK"
    git log --oneline -1
else
    echo "!! push failed (exit $rc)"
fi
exit $rc
'@

$p2 = 'C:\SWGGenesis\_commit_do.sh'
[IO.File]::WriteAllText($p2, ($doit -replace "`r`n", "`n"))
& wsl.exe -d $Distro -u root -- bash /mnt/c/SWGGenesis/_commit_do.sh 2>&1 | ForEach-Object { Write-Host $_ }

Say ''
Say '  ============================================================' Cyan
if ($LASTEXITCODE -eq 0) { Say '   Committed and pushed to SWGfan.' Green }
else { Say '   Push did not complete - see the output above.' Yellow }
Say '  ============================================================' Cyan
Say ''

# =====================================================================
#  Fix the buffer overflow that kills the server at boot
#  Written by SWGReturn (Claude), 2026-08-15.
#
#  THE BUG  (engine3: system/lang/Time.h, getFormattedTimeFull)
#
#      char buf[128];
#      int len = sizeof(buf);                              // 128
#      ret = strftime(buf, len, "%Y-%m-%dT%H:%M:%S", &t);  // ret = 19
#      len -= ret - 1;                                     // 110  <-- WRONG
#      snprintf(&buf[ret], len, ".%09ld", ts.tv_nsec);     // only 109 left
#
#  Space remaining at &buf[19] is 128-19 = 109, but len says 110. The
#  "- 1" is simply wrong. Ubuntu 24.04 compiles with _FORTIFY_SOURCE=3
#  by default, glibc sees 110 > 109 and calls abort():
#
#      *** buffer overflow detected ***: terminated
#      #7 snprintf (__fmt=".%09ld", __n=110)
#      #8 sys::lang::Time::getFormattedTimeFull  (Time.h:254)
#      #9 PlayerManagerImplementation::logOnlinePlayers (:6540)
#
#  Debian (the old setup) does not fortify by default, so the same
#  write happened silently there. This is a real bug, not a false alarm.
#
#  THE FIX: len -= ret - 1;   ->   len -= ret;
#
#  Time.h is pulled in via system/lang.h by nearly every translation
#  unit, so this needs a full rebuild. Expect 50-65 minutes.
# =====================================================================

$ErrorActionPreference = 'Continue'
$Distro = 'Ubuntu-24.04'
$Bin    = 'C:\SWGGenesis\MMOCoreORB\bin\core3'

function Say($m, $c = 'Gray') { Write-Host $m -ForegroundColor $c }

Clear-Host
Say ''
Say '  ############################################################' Cyan
Say '  #   Fix the boot crash, then rebuild                       #' Cyan
Say '  ############################################################' Cyan
Say ''
Say '  One-character fix in engine3, then a full rebuild.' Gray
Say '  Expect 50-65 minutes. You can leave this running.' Gray
Say ''

$bash = @'
#!/usr/bin/env bash
set -uo pipefail

REPO=/mnt/c/SWGGenesis
TIMEH=$REPO/MMOCoreORB/utils/engine3/MMOEngine/src/system/lang/Time.h
BUILD=$REPO/MMOCoreORB/build/unix
LOG=$REPO/fix_rebuild_log.txt

exec > >(tee -a "$LOG") 2>&1
echo
echo "============ fix + rebuild  $(date) ============"

if [ ! -f "$TIMEH" ]; then echo "!! Time.h not found at $TIMEH"; exit 1; fi

echo
echo "-------- patching Time.h --------"
if grep -q 'len -= ret - 1;' "$TIMEH"; then
    cp -n "$TIMEH" "$TIMEH.bak-overflowfix"
    sed -i 's/len -= ret - 1;/len -= ret;        \/\* 2026-08-15: was "ret - 1", which overstated the remaining space in buf[128] by one byte and tripped _FORTIFY_SOURCE=3 on Ubuntu. *\//' "$TIMEH"
    echo "  patched:"
    grep -n 'len -= ret;' "$TIMEH"
elif grep -q 'len -= ret;' "$TIMEH"; then
    echo "  already patched, nothing to do"
else
    echo "!! could not find the line to patch -- aborting rather than guessing"
    exit 1
fi

echo
echo "-------- rebuild --------"
cd "$BUILD" || exit 1
NPROC=$(nproc)
echo "  make -j$NPROC  (Time.h is included nearly everywhere, so this is a full rebuild)"
make -j"$NPROC"
RC=$?

echo
echo "-------- result --------"
if [ -x "$REPO/MMOCoreORB/bin/core3" ]; then
    ls -lh "$REPO/MMOCoreORB/bin/core3"
    echo
    echo "BUILD OK"
    exit 0
else
    echo "!! core3 missing (make exit $RC)"
    grep -E "error:" "$LOG" | tail -20
    exit 1
fi
'@

$path = 'C:\SWGGenesis\fix_time_overflow.sh'
[IO.File]::WriteAllText($path, ($bash -replace "`r`n", "`n"))

& wsl.exe -d $Distro -u root -- bash /mnt/c/SWGGenesis/fix_time_overflow.sh 2>&1 |
    ForEach-Object { Write-Host $_ }

$code = $LASTEXITCODE
Say ''
Say '  ============================================================' Cyan
if ($code -eq 0 -and (Test-Path $Bin)) {
    $sz = [math]::Round((Get-Item $Bin).Length / 1MB, 1)
    Say ("   FIXED AND REBUILT -- core3 is {0} MB" -f $sz) Green
    Say ''
    Say '   Now open the control panel and press Start.' Gray
} else {
    Say '   Something went wrong. Send Claude:' Yellow
    Say '     C:\SWGGenesis\fix_rebuild_log.txt' Yellow
}
Say '  ============================================================' Cyan
Say ''

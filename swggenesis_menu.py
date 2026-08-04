#!/usr/bin/env python3
"""
SWG Genesis server backend -- called by SWGGenesisControlPanel.ahk via wsl.exe.

Lives on the Windows D: drive and is executed directly from WSL, so there is
no copy step: /mnt/d/SWGGenesis/swggenesis_menu.py

Every action prints plain text to stdout. The GUI parses `status`, `remote`,
`planets_get` and `backup_count`; everything else is free text for the log pane.
Run with no args for a list of actions.

Forked 2026-08-03 from swgreturn_menu.py v2 for the genesis base
(upstream 3f445ff1f2 "swap to genesis code as base"). Differences that matter:

  * config-local.lua uses DOTTED keys -- genesis wraps config.lua in a
    `Core3 = { ... }` table, so overrides are `Core3.ZonesEnabled = {...}`.
    The zone parser handles both spellings.
  * The client folder is the LAUNCHER's install, which ships pointed at THEIR
    LIVE SERVER. So there is no blind `fix_ip`: `target_local` / `target_live`
    switch deliberately, back up first, and `show_target` reads without writing.
  * cmake needs -DCOMPILE_TESTS=OFF. Genesis's .gitignore has a `*test*`
    pattern that excludes utils/googletest-release-1.10.0 from the repo, while
    CMakeLists.txt:191 still add_subdirectory()s it. A fresh clone cannot
    configure with default options.
"""

import os
import re
import subprocess
import sys
import time
from datetime import datetime

# ---------------------------------------------------------------- configuration
REPO      = "/mnt/d/SWGGenesis"
MMO       = REPO + "/MMOCoreORB"
BIN       = MMO + "/bin"
BUILD     = MMO + "/build/unix"
LOG       = BIN + "/log/core3.log"
CONSOLE_LOG = "/mnt/d/SWGGenesis/console.log"  # stdout-only errors land here
CONF      = BIN + "/conf/config-local.lua"
CONF_MAIN = BIN + "/conf/config.lua"
CLIENT    = "/mnt/d/Launcher/newreturnbenserver"
BACKUPS   = "/mnt/d/SWGGenesis/backups"

SCREEN    = "genesis"
LOGIN_PORT = 46453
ZONE_PORT  = 46463
PING_PORT  = 46462

DB_NAME   = "genesis"
DB_USER   = "returns"
DB_PASS   = "ReturnsDB2026"
GALAXY_ID = 3

CMAKE_FLAGS = [
    "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
    "-DBUILD_IDL=ON",
    "-DCOMPILE_TESTS=OFF",   # see module docstring -- without it cmake cannot configure
    "-DENABLE_ERROR_ON_WARNINGS=OFF",
    "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
    "-Wno-dev",
]

# Only these two carry loginServerAddress0/Port0 in the launcher's install.
CLIENT_CFGS = ["swgemu.cfg", "swgemu_login.cfg"]

# Their production server, as shipped by the launcher. Kept so `target_live`
# can put the client back exactly where it started.
LIVE_ADDR = "74.208.80.130"
LIVE_PORT = 44453


# ---------------------------------------------------------------- shell helpers
def sh(cmd, timeout=None):
    """Run a shell command, return (rc, combined output).

    executable="/bin/bash" is REQUIRED, not cosmetic: shell=True defaults to
    /bin/sh (dash on Debian), which does not support $'...' ANSI-C quoting.
    Without it, `screen -X stuff $'save\\r'` silently fails and console commands
    never reach the server -- which made every graceful shutdown fall through
    to SIGKILL.
    """
    try:
        p = subprocess.run(cmd, shell=True, executable="/bin/bash",
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           timeout=timeout)
        return p.returncode, p.stdout.decode("utf-8", "replace")
    except subprocess.TimeoutExpired:
        return 124, "(timed out after %ss)" % timeout


def mysql(sql, timeout=60):
    """Run SQL as the returns user -- no sudo, so the GUI never blocks on a password."""
    q = sql.replace('"', '\\"')
    return sh('mysql -u %s -p%s %s -e "%s"' % (DB_USER, DB_PASS, DB_NAME, q), timeout)


def screen_exists():
    rc, out = sh("screen -ls")
    return re.search(r"\.%s\s" % re.escape(SCREEN), out) is not None


def core3_pid():
    """PID of THIS server's core3, or None.

    A bare `pgrep -x core3` is wrong here and was a real hazard: the old
    SWGReturn server runs a binary with the same name, so pgrep would report it
    as ours, `status` would show RUNNING when genesis was stopped, and -- much
    worse -- the SIGTERM/SIGKILL escalation in shutdown would have killed the
    OTHER server. We disambiguate by working directory: each server runs from
    its own MMOCoreORB/bin."""
    rc, out = sh("pgrep -x core3")
    for pid in [l.strip() for l in out.splitlines() if l.strip().isdigit()]:
        rc2, cwd = sh("readlink -f /proc/%s/cwd 2>/dev/null" % pid)
        if cwd.strip().startswith(BIN):
            return pid
    return None


def listening_ports():
    rc, out = sh("ss -lnup 2>/dev/null")
    found = []
    for port in (LOGIN_PORT, PING_PORT, ZONE_PORT):
        if re.search(r":%d\s" % port, out):
            found.append(port)
    return found


def stuff(text):
    """Type a command into the live server console, as if you'd typed it yourself."""
    safe = text.replace("'", "'\\''")
    return sh("screen -S %s -p 0 -X stuff $'%s\\r'" % (SCREEN, safe))


def snapshot():
    """Read the server console's current screen + scrollback."""
    tmp = "/tmp/genesis_hardcopy_%d.txt" % int(time.time() * 1000)
    sh("screen -S %s -p 0 -X hardcopy -h %s" % (SCREEN, tmp))
    if not os.path.exists(tmp):
        return ""
    try:
        with open(tmp, "r", encoding="utf-8", errors="replace") as f:
            data = f.read()
    finally:
        try:
            os.remove(tmp)
        except OSError:
            pass
    return "\n".join([l for l in data.splitlines() if l.strip()])


def uptime_of_last(pattern, text):
    """Latest '(N s)' stamp on lines matching pattern, or -1. Used to tell a fresh
    save confirmation from a leftover one."""
    last = -1
    for m in re.finditer(r"\((\d+) s\)[^\n]*" + pattern, text):
        last = max(last, int(m.group(1)))
    return last


def backup_count():
    """How many times the server has reported a completed backup, read from the
    LOG rather than the console. The console is a fixed-size screen buffer whose
    scrollback rolls, so a confirmation can scroll away before we look; the log
    is append-only and never loses it.

    The '.*' is load-bearing. Until 2026-08-03 this matched the literal string
    '[ObjectBrokerAgent] backup finished', but the log actually reads
    '[ObjectBrokerAgent] INFO - backup finished'. It therefore returned 0 against
    a log holding 90 real saves -- which meant Save World Now could never confirm
    and every graceful shutdown fell through to 'no backup finished seen in 120s,
    shutting down anyway'. Matching loosely between the tag and the message keeps
    this working if the log level or separator changes again."""
    rc, out = sh("grep -cE '\\[ObjectBrokerAgent\\].*backup finished' %s 2>/dev/null" % LOG)
    try:
        return int(out.strip() or 0)
    except ValueError:
        return 0


def console_alive():
    """True if the console is accepting commands -- i.e. the server reached READY.
    Sending 'save' during the 7-15 minute boot does nothing at all."""
    return "READY" in snapshot() or LOGIN_PORT in listening_ports()


# ---------------------------------------------------------------------- actions
def act_status():
    pid = core3_pid()
    ports = listening_ports()
    sess = screen_exists()

    if pid and ports:
        extra = "pid %s, listening on %s" % (pid, ", ".join(str(p) for p in ports))
        if LOGIN_PORT not in ports:
            print("SESSION_NO_PROC|pid %s but login port %d is NOT listening "
                  "(startup incomplete or an exception aborted init)" % (pid, LOGIN_PORT))
            return
        print("RUNNING|" + extra)
    elif pid and not ports:
        print("SESSION_NO_PROC|pid %s alive but no ports listening yet -- still booting, "
              "or crashed into gdb" % pid)
    elif sess and not pid:
        print("SESSION_NO_PROC|screen session '%s' exists but no core3 process "
              "(likely crashed -- View Live Console to see the gdb prompt)" % SCREEN)
    else:
        print("STOPPED|no core3 process, no screen session")


def act_start(reloadstrings=False):
    if core3_pid():
        print("Already running (pid %s). Nothing to do." % core3_pid())
        return
    if screen_exists():
        print("A stale screen session '%s' exists with no server in it. Clearing it first." % SCREEN)
        sh("screen -X -S %s quit" % SCREEN)
        time.sleep(2)

    for d in ["log", "log/clients", "log/packets", "log/admin", "log/wal",
              "databases", "navmeshes", "exports"]:
        os.makedirs(os.path.join(BIN, d), exist_ok=True)

    # A TreFiles override that lists an archive no longer shipped upstream kills
    # the boot ~90 seconds in, with an error that reads like a content problem
    # rather than a config one. Cheap to check here; warn, don't block, since a
    # deliberate hand-edit is legitimate.
    _warn_tre_drift()

    # screen -L -Logfile captures EVERYTHING the console prints to a real file.
    # Core3 writes some errors only to stdout and never to core3.log -- the MySQL
    # schema errors (1054/1364) and the ObjectManager "unknown objectcrc /
    # template:<unregistered>" / SchematicMap errors are stdout-only. Without
    # this they exist solely in a fixed-size screen scrollback and are lost.
    if os.path.exists(CONSOLE_LOG):
        try:
            os.replace(CONSOLE_LOG, CONSOLE_LOG + ".prev")
        except OSError:
            pass

    runcmd = "run reloadstrings" if reloadstrings else "run"
    rc, out = sh("cd %s && screen -dmS %s -L -Logfile %s gdb -q -ex '%s' ./core3"
                 % (BIN, SCREEN, CONSOLE_LOG, runcmd))
    time.sleep(3)
    print("Server launching in screen session '%s'%s." %
          (SCREEN, " with reloadstrings" if reloadstrings else ""))
    print("")
    print("First boot after a database wipe takes ~15 minutes; a warm boot ~7.")
    print("Status stays 'booting' until the login port opens -- that is normal.")
    if out.strip():
        print("")
        print(out.strip())


def act_shutdown():
    if not core3_pid():
        print("Server is not running.")
        if screen_exists():
            sh("screen -X -S %s quit" % SCREEN)
            print("Cleared the leftover screen session.")
        return

    if not console_alive():
        print("The server is still booting -- the console isn't accepting commands yet.")
        print("Nothing to save (no players have connected). Stopping it directly.")
    else:
        base = backup_count()
        print("Asking the server to save the world before shutting down...")
        stuff("save")

        saved = False
        for _ in range(80):                  # up to 120s
            time.sleep(1.5)
            if backup_count() > base:
                saved = True
                break
        print("Save confirmed." if saved
              else "WARNING: no 'backup finished' seen in 120s -- shutting down anyway.")

    print("Sending shutdown to the console...")
    stuff("exit")
    for _ in range(40):                      # up to 60s
        time.sleep(1.5)
        if not core3_pid():
            break

    # Signal OUR pid explicitly. `pkill -x core3` would match the other server
    # running from a different directory and take it down with us.
    pid = core3_pid()
    if pid:
        print("Console shutdown did not take. Sending SIGTERM to pid %s "
              "(database was already saved)." % pid)
        sh("kill -TERM %s" % pid)
        time.sleep(5)
    pid = core3_pid()
    if pid:
        print("Still alive -- sending SIGKILL to pid %s." % pid)
        sh("kill -KILL %s" % pid)
        time.sleep(2)

    sh("screen -X -S %s quit" % SCREEN)
    print("")
    print("Server stopped." if not core3_pid() else "Server may still be running -- check status.")


def act_restart():
    act_shutdown()
    time.sleep(3)
    act_start()


def act_rebuild():
    if core3_pid():
        print("The server is RUNNING. Stop it first -- a build while it runs will fight for RAM.")
        return
    print("Configuring (cmake)...")
    rc, out = sh("cd %s && cmake %s ../.. 2>&1 | tail -5" % (BUILD, " ".join(CMAKE_FLAGS)),
                 timeout=900)
    print(out.strip())
    if rc != 0:
        print("\nCMAKE FAILED -- stopping here.")
        return

    print("\nCompiling (make -j4). A full rebuild takes about an hour on the D: drive...")
    rc, out = sh("cd %s && make -j4 2>&1 | tail -25" % BUILD, timeout=14400)
    print(out.strip())
    if rc == 0:
        print("\n=== BUILD OK ===")
    else:
        print("\n=== BUILD FAILED -- first error below ===")
        rc2, err = sh("cd %s && make -j4 2>&1 | grep -m1 -B4 -A15 'error:'" % BUILD, timeout=600)
        print(err.strip() or "(no 'error:' line found -- see output above)")


def act_console_snapshot():
    s = snapshot()
    print(s if s.strip() else "(no console output -- server not running?)")


def act_console():
    os.execvp("screen", ["screen", "-r", SCREEN])


def act_log_tail(n=80):
    rc, out = sh("tail -n %d %s" % (n, LOG))
    print(out.strip() or "(log empty)")


def act_console_errors():
    """Errors Core3 prints ONLY to stdout -- never to core3.log. Requires the
    server to have been started by this script (screen -L writes CONSOLE_LOG)."""
    if not os.path.exists(CONSOLE_LOG):
        print("No console log yet at %s" % CONSOLE_LOG)
        print("It is created by `start`. If the running server was started another")
        print("way, use console_snapshot instead (scrollback only, may have rolled).")
        return

    rc, sz = sh("du -h %s | cut -f1" % CONSOLE_LOG)
    print("Console log: %s (%s)\n" % (CONSOLE_LOG, sz.strip()))

    for label, pattern in [
        ("unregistered object CRCs", "unknown objectcrc"),
        ("schematic creation failures", "Could not create schematic"),
        ("MySQL errors", "^[0-9]\\{4\\}: "),
        ("segfaults / aborts", "SIGSEGV\\|SIGABRT\\|Segmentation"),
        ("missing templates", "not found"),
    ]:
        rc, c = sh("grep -c '%s' %s 2>/dev/null" % (pattern, CONSOLE_LOG))
        print("  %-28s %s" % (label, c.strip() or "0"))

    print("\n--- distinct failing schematic CRCs ---")
    rc, out = sh("grep -o 'schematic with crc: [0-9]*' %s | sort -u" % CONSOLE_LOG)
    print(out.strip() or "(none)")

    print("\n--- distinct unregistered object CRCs ---")
    rc, out = sh("grep -o 'objectcrc 0x[0-9a-f]*' %s | sort -u" % CONSOLE_LOG)
    print(out.strip() or "(none)")

    print("\n--- distinct MySQL errors ---")
    rc, out = sh("grep -oE '^[0-9]{4}: .*' %s | sort -u | head -20" % CONSOLE_LOG)
    print(out.strip() or "(none)")


def act_log_errors():
    rc, out = sh("grep -nE 'ERROR|SIGSEGV|Segmentation|FATAL|exception' %s | tail -60" % LOG)
    body = out.strip()
    rc, cnt = sh("grep -cE 'ERROR|SIGSEGV|Segmentation|FATAL|exception' %s" % LOG)
    rc, miss = sh("grep -c 'not found' %s" % LOG)
    print("Total error-class lines in log: %s" % cnt.strip())
    print("Missing-template warnings:      %s" % miss.strip())
    print("")
    print("--- last 60 error-class lines ---")
    print(body or "(none -- clean log)")


def act_accounts():
    rc, out = mysql("SELECT account_id, username, admin_level, active, created "
                    "FROM accounts ORDER BY account_id;")
    print(out.strip() or "(no accounts yet)")
    print("")
    rc, out = mysql("SELECT COUNT(*) AS characters FROM characters;")
    print(out.strip())


def act_set_admin(user, level="15"):
    rc, out = mysql("UPDATE accounts SET admin_level=%s WHERE username='%s';" %
                    (int(level), user.replace("'", "")))
    print(out.strip())
    rc, out = mysql("SELECT account_id, username, admin_level FROM accounts "
                    "WHERE username='%s';" % user.replace("'", ""))
    print(out.strip() or "No account named '%s'. Usernames are stored lowercase." % user)


def act_db_backup():
    os.makedirs(BACKUPS, exist_ok=True)
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    dest = "%s/%s_%s.sql.gz" % (BACKUPS, DB_NAME, stamp)
    print("Dumping %s ..." % DB_NAME)
    rc, out = sh("mysqldump -u %s -p%s --single-transaction --routines %s | gzip > %s"
                 % (DB_USER, DB_PASS, DB_NAME, dest), timeout=1800)
    if rc != 0:
        print("Backup FAILED:\n" + out.strip())
        return
    rc, sz = sh("du -h %s | cut -f1" % dest)
    print("Wrote %s (%s)" % (dest, sz.strip()))
    print("")
    print("NOTE: this backs up the MySQL side only. The object database lives in")
    print("      %s/databases and is a separate Berkeley DB store." % BIN)
    rc, out = sh("ls -1t %s | head -10" % BACKUPS)
    print("\nMost recent backups:\n" + out.strip())


def wsl_ip():
    rc, out = sh("hostname -I | awk '{print $1}'")
    return out.strip()


def _set_client_target(addr, port, label):
    """Point the launcher's client cfgs at addr:port.

    ⚠ This folder is the LAUNCHER'S install and ships pointed at THEIR LIVE
    SERVER. Unlike the old SWGReturn setup, where the client folder was ours
    alone, writing here changes which server Nick can play. So: back up once
    before the first write, and always print both the before and after so the
    change is visible rather than assumed.

    Both the address and the port are enforced. The port matters as much as the
    address -- their live server is 44453, which is also Companion's login port,
    so a half-applied change silently connects to the wrong server with no error
    at all, just an endless "Connecting to the Login Server...".
    """
    print("Pointing the client at %s  (%s:%d)\n" % (label, addr, port))
    for cfg in CLIENT_CFGS:
        path = os.path.join(CLIENT, cfg)
        if not os.path.exists(path):
            print("  missing: %s" % path)
            continue

        # One pristine backup, taken before we ever touch the file. Suffixed
        # distinctly so it cannot collide with the .bak files the launcher
        # writes for itself.
        bak = path + ".genesis-panel.bak"
        if not os.path.exists(bak):
            sh("cp %s %s" % (path, bak))
            print("  backed up %s -> %s" % (cfg, os.path.basename(bak)))

        rc, before = sh("grep -E 'loginServerAddress0|loginServerPort0' %s | tr -d '\\r' | tr '\\n' ' '" % path)
        sh("sed -i -E 's/^([[:space:]]*loginServerAddress0=).*/\\1%s/' %s" % (addr, path))
        sh("sed -i -E 's/^([[:space:]]*loginServerPort0=).*/\\1%d/' %s" % (port, path))
        rc, after = sh("grep -E 'loginServerAddress0|loginServerPort0' %s | tr -d '\\r' | tr '\\n' ' '" % path)
        print("  %-22s was: %s" % (cfg, before.strip() or "(no login lines)"))
        print("  %-22s now: %s" % ("", after.strip() or "(no login lines)"))


def act_target_local():
    """Play THIS server. Rewrites the client cfgs and the galaxy row."""
    ip = wsl_ip()
    if not ip:
        print("Could not determine the WSL IP.")
        return
    _set_client_target(ip, LOGIN_PORT, "your local genesis server")
    mysql("UPDATE galaxy SET address='%s' WHERE galaxy_id=%d;" % (ip, GALAXY_ID))
    rc, out = mysql("SELECT galaxy_id, name, address, port, pingport FROM galaxy;")
    print("\nGalaxy row now:\n" + out.strip())
    print("\nRestart the game client (not the server) for this to take effect.")


def act_target_live():
    """Put the client back on THEIR live server, exactly as the launcher had it.

    The galaxy row is deliberately NOT touched -- it describes our own server and
    means nothing to theirs."""
    _set_client_target(LIVE_ADDR, LIVE_PORT, "SWG Returns LIVE (their server)")
    print("\nRestart the game client. Your local server is unaffected;")
    print("switch back with 'Play Local' whenever you want.")


def act_show_target():
    """Read-only. What the client points at, and what it would need for local."""
    ip = wsl_ip() or "(unknown)"
    print("WSL IP right now : %s" % ip)
    print("Local genesis    : %s:%d   (login, UDP)" % (ip, LOGIN_PORT))
    print("Their live server: %s:%d" % (LIVE_ADDR, LIVE_PORT))
    # tr -d '\\r' below is load-bearing: these cfgs are Windows files with CRLF
    # endings. A stray carriage return snaps the cursor back to column 0 and
    # overwrites the filename we just printed, so the line reads as though the
    # filename were missing and the fields were in the wrong order. The data was
    # always correct -- the terminal was eating it.
    print("\n--- what the client cfgs say ---")
    pointing = set()
    for cfg in CLIENT_CFGS:
        path = os.path.join(CLIENT, cfg)
        if not os.path.exists(path):
            print("  %-22s (missing)" % cfg)
            continue
        rc, chk = sh("grep -E 'loginServerAddress0|loginServerPort0' %s | tr -d '\\r' | tr '\\n' ' '" % path)
        print("  %-22s %s" % (cfg, chk.strip() or "(no login lines)"))
        if LIVE_ADDR in chk:
            pointing.add("LIVE")
        elif ip != "(unknown)" and ip in chk:
            pointing.add("LOCAL")
        else:
            pointing.add("OTHER")

    verdict = {frozenset(["LIVE"]): "their LIVE server",
               frozenset(["LOCAL"]): "your local genesis server"}.get(frozenset(pointing))
    print("\nCurrently pointing at: %s" % (verdict or
          "MIXED or UNKNOWN -- the cfgs disagree, or the IP has changed since "
          "they were written. Press Play Local or Play Live to make it definite."))
    print("Nothing was changed by this action.")


def act_fix_ip():
    """Kept as an alias for muscle memory from the SWGReturn panel."""
    print("(fix_ip is 'target_local' on this server -- the client folder is shared")
    print(" with their live server, so switching is explicit here.)\n")
    act_target_local()


def _act_fix_ip_legacy():
    rc, out = sh("hostname -I | awk '{print $1}'")
    ip = out.strip()
    if not ip:
        print("Could not determine the WSL IP.")
        return
    print("WSL IP is now: %s" % ip)

    rc, out = mysql("SELECT address FROM galaxy WHERE galaxy_id=%d;" % GALAXY_ID)
    print("\nGalaxy row before:\n" + out.strip())

    # Enforce BOTH address and port. The port matters as much as the address:
    # Returns' own default is 44453, which is also Companion's login port, so a
    # restored .orig or a launcher run silently points the client at the WRONG
    # SERVER rather than at nothing. Symptom is an endless "Connecting to the
    # Login Server..." with no error.
    for cfg in CLIENT_CFGS:
        path = os.path.join(CLIENT, cfg)
        if not os.path.exists(path):
            print("  missing: %s" % path)
            continue
        sh("sed -i -E 's/^([[:space:]]*loginServerAddress0=).*/\\1%s/' %s" % (ip, path))
        sh("sed -i -E 's/^([[:space:]]*loginServerPort0=).*/\\1%d/' %s" % (LOGIN_PORT, path))
        rc, chk = sh("grep -E 'loginServerAddress0|loginServerPort0' %s | tr -d '\\r' | tr '\\n' ' '" % path)
        print("  %-30s %s" % (cfg, chk.strip()))

    mysql("UPDATE galaxy SET address='%s' WHERE galaxy_id=%d;" % (ip, GALAXY_ID))
    rc, out = mysql("SELECT galaxy_id, name, address, port, pingport FROM galaxy;")
    print("\nGalaxy row after:\n" + out.strip())
    print("\nRestart the game client (not the server) for it to pick this up.")


def act_diff():
    rc, out = sh("cd %s && git status --short | head -60" % REPO)
    print("--- changed files ---")
    print(out.strip() or "(working tree clean)")
    rc, out = sh("cd %s && git diff --stat | tail -20" % REPO)
    print("\n--- diff stat ---")
    print(out.strip() or "(no tracked-file changes)")


def act_pull():
    rc, out = sh("cd %s && git remote -v && echo '---' && git fetch upstream 2>&1 | tail -5" % REPO)
    print(out.strip())
    print("\nFetched from upstream. NOT merged -- their main branch has repeatedly")
    print("shipped code that does not compile. Review before merging.")


def act_push(msgfile=None):
    msg = "Genesis updates from DESKTOP-2MI669I"
    if msgfile and os.path.exists(msgfile):
        # utf-8-sig strips a leading BOM if one is present -- some editors and
        # AHK's FileOpen(..., "UTF-8") write one, and it would otherwise become
        # an invisible first character in the commit message.
        with open(msgfile, "r", encoding="utf-8-sig", errors="replace") as f:
            msg = f.read().strip().lstrip("﻿") or msg
    rc, out = sh("cd %s && git remote get-url origin" % REPO)
    if rc != 0:
        print("No 'origin' remote is configured yet.")
        print("Create the fork at github.com/SWGfan/SWGReturns, then:")
        print("  cd %s && git remote add origin https://github.com/SWGfan/SWGReturns.git" % REPO)
        return
    esc = msg.replace('"', '\\"')
    rc, out = sh('cd %s && git add -A && git commit -m "%s" 2>&1 | tail -5 && '
                 'git push origin HEAD 2>&1 | tail -10' % (REPO, esc), timeout=1800)
    print(out.strip())


def act_ports():
    rc, out = sh("ss -lnup 2>/dev/null | grep -E '46419|46453|46455|46460|46462|46463'")
    print("--- Genesis (46xxx) ---")
    print(out.strip() or "(nothing listening)")
    rc, out = sh("ss -lnup 2>/dev/null | grep -E '44419|44453|45419|45453'")
    print("\n--- Companion 44xxx / old SWGReturn 45xxx, for comparison ---")
    print(out.strip() or "(nothing listening)")


def act_show_ip():
    """Alias kept for muscle memory -- show_target is the real one here."""
    act_show_target()


def _act_show_ip_legacy():
    """Superseded by show_target, which also reports WHICH server the client is
    currently pointing at -- the question that matters when the client folder is
    shared with their live service."""
    rc, out = sh("hostname -I | awk '{print $1}'")
    ip = out.strip() or "(unknown)"
    print("Current WSL IP : %s" % ip)
    print("Login  port    : %d   (UDP)" % LOGIN_PORT)
    print("Ping   port    : %d   (UDP)" % PING_PORT)
    print("Zone   port    : %d   (UDP)" % ZONE_PORT)
    print("")
    print("--- what the client cfgs say right now ---")
    for cfg in CLIENT_CFGS:
        path = os.path.join(CLIENT, cfg)
        if not os.path.exists(path):
            print("  %-30s (missing)" % cfg)
            continue
        rc, chk = sh("grep -E 'loginServerAddress0|loginServerPort0' %s | tr -d '\\r' | tr '\\n' ' '" % path)
        print("  %-30s %s" % (cfg, chk.strip() or "(no login lines)"))
    rc, out = mysql("SELECT galaxy_id, name, address, port, pingport FROM galaxy;")
    print("\n--- galaxy row ---\n" + out.strip())
    print("\nNothing was changed. Use 'Fix Client IP' to write these values.")


def act_backup_count():
    """Integer only. The GUI polls this after typing 'save' and watches for it to
    increase. Counted from core3.log rather than the console: the console is a
    fixed-size screen buffer whose scrollback rolls, so a confirmation can scroll
    away before the GUI looks. The log is append-only and never loses it."""
    print(backup_count())


# ------------------------------------------------------------------ zone control
# Zone names live in ONE list in their config.lua -- ZonesEnabled -- with the
# space_* zones mixed in alongside the ground ones, and disabled zones left in
# place as `--"name",` comments. We read that block for the full universe of
# known names, and WRITE to config-local.lua rather than config.lua.
#
# Writing to config-local.lua is deliberate and matters more now than it did:
# bin/conf/* is gitignored, so an override there survives `git pull` from
# upstream. Editing config.lua directly would collide with every upstream
# content update, and this fork now tracks upstream closely.
# Zone names are bare quoted identifiers; the lists contain no nested braces,
# which is why matching to the first '}' above is safe for BOTH a multi-line
# block and a one-liner like `Core3.ZonesEnabled = { "tutorial", "tatooine" }`.
ZONE_NAME = re.compile(r'"([A-Za-z0-9_]+)"')


def _read(path):
    if not os.path.exists(path):
        return ""
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return f.read()


def _zone_block(text, key="ZonesEnabled"):
    """Text inside an UNCOMMENTED `key = { ... }`, or None if absent.

    The leading (?!\\s*--) guard matters: config-local.lua ships with a
    commented-out `-- ZonesEnabled = { ... }` example line, and matching that
    would report the example as if it were live configuration."""
    m = re.search(r'^(?![ \t]*--)[ \t]*' + re.escape(key) + r'[ \t]*=[ \t]*\{(.*?)\}',
                  text, re.S | re.M)
    return m.group(1) if m else None


# Genesis wraps config.lua in `Core3 = { ... }`, so an override in
# config-local.lua must be written `Core3.ZonesEnabled = {...}`. The pre-genesis
# base used a bare `ZonesEnabled`. We READ both and always WRITE the dotted form.
# (re.escape above is why this is safe: an unescaped '.' would match anything.)
LOCAL_ZONE_KEY = "Core3.ZonesEnabled"


def _local_zone_block(text):
    """The live zone list from config-local.lua, dotted form preferred."""
    for key in (LOCAL_ZONE_KEY, "ZonesEnabled"):
        blk = _zone_block(text, key)
        if blk is not None:
            return blk
    return None


def _parse_zones(block):
    """[(name, enabled)] in file order. A `--"name"` line is a disabled zone.

    Handles three shapes seen in the wild:
        "tatooine",
        --"otoh_gunga",
        "tutorial",   -- comma here          <- trailing comment
    and any number of names on one line, which is how a hand-written
    `Core3.ZonesEnabled = { "a", "b", "c" }` override arrives."""
    out = []
    if not block:
        return out
    for line in block.splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        commented = stripped.startswith("--")
        if commented:
            code = stripped.lstrip("-").strip()
        else:
            code = stripped.split("--", 1)[0]   # drop any trailing comment
        for m in ZONE_NAME.finditer(code):
            out.append((m.group(1), not commented))
    return out


def _zone_type(name):
    return "SPACE" if name.startswith("space_") else "GROUND"


def act_planets_get():
    """TYPE|name|0or1 per line. config.lua supplies the universe of known zone
    names; an override in config-local.lua, if present, decides which are on."""
    universe = _parse_zones(_zone_block(_read(CONF_MAIN)))
    if not universe:
        print("(could not read ZonesEnabled from %s)" % CONF_MAIN)
        return

    local = _local_zone_block(_read(CONF))
    if local is not None:
        on = {n for n, en in _parse_zones(local) if en}
        known = {n for n, _ in universe}
        for n in on - known:                      # local-only names still listed
            universe.append((n, True))
        universe = [(n, n in on) for n, _ in universe]

    for name, enabled in universe:
        print("%s|%s|%d" % (_zone_type(name), name, 1 if enabled else 0))


def act_planets_set(ground_csv, space_csv):
    ground = [z for z in (ground_csv or "").split(",") if z.strip()]
    space = [z for z in (space_csv or "").split(",") if z.strip()]

    known = {n for n, _ in _parse_zones(_zone_block(_read(CONF_MAIN)))}
    unknown = [z for z in ground + space if z not in known]
    if unknown:
        print("REFUSED: unknown zone name(s): %s" % ", ".join(unknown))
        print("Only names that appear in %s can be enabled." % CONF_MAIN)
        return
    if not ground:
        print("REFUSED: at least one ground zone must stay enabled -- "
              "a server with none will not boot.")
        return

    text = _read(CONF)
    if not text:
        print("REFUSED: %s is missing. Not creating it from scratch -- it holds "
              "the database credentials and the TRE list." % CONF)
        return

    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    # Two Apply presses inside the same second would otherwise land on the same
    # filename and the second would overwrite the first backup -- which is the
    # one you would want, since it holds the state before you started.
    bak = "%s.%s.bak" % (CONF, stamp)
    n = 1
    while os.path.exists(bak):
        bak = "%s.%s_%d.bak" % (CONF, stamp, n)
        n += 1
    with open(bak, "w", encoding="utf-8") as f:
        f.write(text)

    ordered = [z for z, _ in _parse_zones(_zone_block(_read(CONF_MAIN)))
               if z in set(ground + space)]
    body = "\n".join('    "%s",' % z for z in ordered).rstrip(",")
    block = ("-- Managed by the control panel's Planets window. Edited %s.\n"
             "%s = {\n%s\n}\n" % (stamp, LOCAL_ZONE_KEY, body))

    existing = None
    for key in (LOCAL_ZONE_KEY, "ZonesEnabled"):
        if _zone_block(text, key) is not None:
            existing = key
            break

    if existing:
        new = re.sub(r'^(?:[ \t]*--[^\n]*\n)?(?![ \t]*--)[ \t]*' + re.escape(existing)
                     + r'[ \t]*=[ \t]*\{.*?\}[ \t]*\n?',
                     block, text, count=1, flags=re.S | re.M)
    else:
        new = text.rstrip() + "\n\n" + block

    with open(CONF, "w", encoding="utf-8") as f:
        f.write(new)

    print("Wrote %d zone(s) to config-local.lua  (%d ground, %d space)."
          % (len(ordered), len(ground), len(space)))
    print("Backup: %s" % bak)
    print("")
    print("Takes effect on the NEXT SERVER START, not immediately.")
    if space:
        print("")
        print("CAUTION: you enabled %d space zone(s). SpaceZonesEnabled is still { }"
              % len(space))
        print("and upstream reverted the JTL/space layer out on 2026-08-02, so space")
        print("zones are UNTESTED on this build. If the server fails to boot, restore")
        print("the backup above and start again.")


# ------------------------------------------------------------------ TRE load order
# The Companion client patch (companion_patch.tre) overrides base archives --
# skills.iff, the command tables, several .stf string files. In SWG the FIRST
# matching archive in TreFiles wins, so the patch is only in effect if it sits
# at the TOP of the list. Being present in the folder is not enough.
#
# Genesis owns that list in config.lua (56 entries as of 2026-08-04) and
# config-local.lua does not override it. Three ways to get our entry first:
#
#   A. Edit config.lua directly       -- one line, but collides with every
#                                        upstream content update, and this fork
#                                        deliberately tracks upstream closely.
#   B. Hand-copy all 56 into          -- works today, silently goes stale the
#      config-local.lua                  moment upstream adds or removes a TRE.
#                                        A missing archive is a boot failure;
#                                        a stale EXTRA one is a silent content
#                                        mismatch, which is worse.
#   C. Regenerate the override from   -- what this does. Idempotent, so it is
#      config.lua on demand              safe to re-run after every `pull`, and
#                                        upstream TRE changes are picked up
#                                        automatically. config.lua stays pristine.
#
# bin/conf/* is gitignored, so the generated override survives a pull; it just
# needs regenerating if the upstream list changed. `tre_check` reports that
# without writing anything.
PATCH_TRE  = "companion_patch.tre"
LOCAL_TRE_KEY = "Core3.TreFiles"
TRE_NAME   = re.compile(r'"([^"]+)"')


def _tre_path():
    """TrePath as the server will resolve it: local override beats config.lua."""
    for path, keys in ((CONF, ("Core3.TrePath", "TrePath")),
                       (CONF_MAIN, ("TrePath",))):
        text = _read(path)
        for key in keys:
            m = re.search(r'^(?![ \t]*--)[ \t]*' + re.escape(key)
                          + r'[ \t]*=[ \t]*"([^"]*)"', text, re.M)
            if m:
                return m.group(1)
    return ""


def _tre_list(text, key):
    """Entries of an uncommented `key = { ... }`, in order, or None."""
    blk = _zone_block(text, key)      # same brace-matching rules; no nested braces
    if blk is None:
        return None
    out = []
    for line in blk.splitlines():
        stripped = line.strip()
        if stripped.startswith("--"):
            continue                  # a commented-out archive is not loaded
        out.extend(TRE_NAME.findall(stripped.split("--", 1)[0]))
    return out


def _local_tre_list(text):
    for key in (LOCAL_TRE_KEY, "TreFiles"):
        got = _tre_list(text, key)
        if got is not None:
            return got, key
    return None, None


def _warn_tre_drift():
    """Called from act_start. Silent when there is nothing to say -- a warning
    printed on every single start is a warning nobody reads."""
    live, _ = _local_tre_list(_read(CONF))
    if live is None:
        return                       # no override at all; config.lua governs
    upstream = _tre_list(_read(CONF_MAIN), "TreFiles")
    if not upstream:
        return
    mine = [t for t in live if t != PATCH_TRE]
    missing = [t for t in upstream if t not in mine]
    extra = [t for t in mine if t not in upstream]
    tre_dir = _tre_path()
    absent = [t for t in live
              if tre_dir and not os.path.exists(os.path.join(tre_dir, t))]
    if not (missing or extra or absent):
        return
    print("WARNING -- TRE load order in config-local.lua looks wrong:")
    if len(absent) == len(live):
        # Every entry missing means the folder is wrong, not the list. Saying
        # "12 archives are missing" would send you editing the wrong file.
        print("   NONE of the %d listed archives exist in TrePath:" % len(live))
        print("     %s" % (tre_dir or "(TrePath not set)"))
        print("   That is a TrePath problem, not a load-order one.")
        absent = []
    for t in absent[:5]:
        print("   listed but NOT in TrePath : %s   <-- this WILL abort the boot" % t)
    if len(absent) > 5:
        print("   ... and %d more not in TrePath" % (len(absent) - 5))
    for t in extra:
        print("   upstream no longer ships  : %s" % t)
    for t in missing:
        print("   upstream added, missing   : %s" % t)
    print("   Fix with:  tre_sync    (then start again)")
    print("")


def act_tre_check():
    """Read-only. Says whether the patch is first, and whether the override has
    drifted from config.lua -- which is the failure mode that bites after a pull."""
    upstream = _tre_list(_read(CONF_MAIN), "TreFiles")
    if upstream is None:
        print("Could not read TreFiles from %s." % CONF_MAIN)
        return
    live, key = _local_tre_list(_read(CONF))
    tre_dir = _tre_path()

    print("config.lua       : %d archive(s)" % len(upstream))
    if live is None:
        print("config-local.lua : no TreFiles override -- config.lua's list is what loads.")
        print("")
        print("=> %s is NOT in the load order. Run:  tre_sync" % PATCH_TRE)
    else:
        print("config-local.lua : %d archive(s), key `%s`" % (len(live), key))
        print("")
        if live and live[0] == PATCH_TRE:
            print("Load order       : OK -- %s is FIRST." % PATCH_TRE)
        elif PATCH_TRE in live:
            print("Load order       : WRONG -- %s is at position %d, not first."
                  % (PATCH_TRE, live.index(PATCH_TRE) + 1))
            print("                   Base archives ahead of it win. Run:  tre_sync")
        else:
            print("Load order       : %s is absent from the override. Run:  tre_sync"
                  % PATCH_TRE)

        # Drift is the whole reason tre_check exists. Compare against upstream
        # ignoring our own prepended entry.
        mine = [t for t in live if t != PATCH_TRE]
        missing = [t for t in upstream if t not in mine]
        extra = [t for t in mine if t not in upstream]
        if missing or extra:
            print("")
            print("DRIFT vs config.lua -- the override is stale, re-run tre_sync:")
            for t in missing:
                print("   upstream added, we are missing : %s" % t)
            for t in extra:
                print("   upstream removed, we still list: %s   <-- server will fail to boot"
                      % t)
        elif mine:
            print("Drift            : none, override matches config.lua.")

    if tre_dir:
        print("")
        print("TrePath          : %s" % tre_dir)
        p = os.path.join(tre_dir, PATCH_TRE)
        if os.path.exists(p):
            print("%-17s: present, %s bytes" % (PATCH_TRE, format(os.path.getsize(p), ",")))
        else:
            print("%-17s: NOT FOUND in TrePath -- listing it would abort the boot."
                  % PATCH_TRE)


def act_tre_sync():
    """Regenerate Core3.TreFiles in config-local.lua = patch first, then
    config.lua's list verbatim. Idempotent; run it after every upstream pull."""
    upstream = _tre_list(_read(CONF_MAIN), "TreFiles")
    if upstream is None:
        print("REFUSED: could not read TreFiles from %s -- nothing to base the "
              "override on." % CONF_MAIN)
        return
    if len(upstream) < 10:
        # A regex that half-matched would produce a short list, and writing that
        # override would leave the server unable to find most of its content.
        print("REFUSED: only parsed %d archive(s) from config.lua. That is too few "
              "to be real -- refusing to write a truncated load order." % len(upstream))
        return

    tre_dir = _tre_path()
    if tre_dir and not os.path.exists(os.path.join(tre_dir, PATCH_TRE)):
        print("REFUSED: %s is not in %s." % (PATCH_TRE, tre_dir))
        print("A TreFiles entry with no file behind it aborts the boot. Copy the")
        print("patch into TrePath first, then run tre_sync again.")
        return

    text = _read(CONF)
    if not text:
        print("REFUSED: %s is missing. Not creating it from scratch -- it holds "
              "the database credentials." % CONF)
        return

    ordered = [PATCH_TRE] + [t for t in upstream if t != PATCH_TRE]

    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    bak = "%s.%s.bak" % (CONF, stamp)
    n = 1
    while os.path.exists(bak):
        bak = "%s.%s_%d.bak" % (CONF, stamp, n)
        n += 1
    with open(bak, "w", encoding="utf-8") as f:
        f.write(text)

    body = "\n".join('    "%s",' % t for t in ordered).rstrip(",")
    block = ("-- Generated by `swggenesis_menu.py tre_sync` on %s.\n"
             "-- = %s first, then config.lua's list verbatim. Re-run after any pull.\n"
             "%s = {\n%s\n}\n" % (stamp, PATCH_TRE, LOCAL_TRE_KEY, body))

    existing = None
    for key in (LOCAL_TRE_KEY, "TreFiles"):
        if _zone_block(text, key) is not None:
            existing = key
            break

    if existing:
        new = re.sub(r'^(?:[ \t]*--[^\n]*\n)*(?![ \t]*--)[ \t]*' + re.escape(existing)
                     + r'[ \t]*=[ \t]*\{.*?\}[ \t]*\n?',
                     block, text, count=1, flags=re.S | re.M)
    else:
        new = text.rstrip() + "\n\n" + block

    with open(CONF, "w", encoding="utf-8") as f:
        f.write(new)

    print("Wrote %d archive(s) to config-local.lua, %s first."
          % (len(ordered), PATCH_TRE))
    print("Backup: %s" % bak)
    print("")
    print("Takes effect on the NEXT SERVER START. Because this changes string")
    print("tables, start once with reloadstrings.")


def act_remote():
    """REMOTE|url|branch|upstream|ahead|behind -- so the GUI can show the push
    target before you press anything, instead of asking you to remember it."""
    rc, url = sh("cd %s && git remote get-url origin 2>/dev/null" % REPO)
    if rc != 0 or not url.strip():
        print("No 'origin' remote is configured in %s." % REPO)
        return
    rc, br = sh("cd %s && git rev-parse --abbrev-ref HEAD" % REPO)
    rc2, up = sh("cd %s && git rev-parse --abbrev-ref --symbolic-full-name @{u} 2>/dev/null" % REPO)
    upstream = up.strip() if rc2 == 0 and up.strip() else "(no upstream set)"

    ahead = behind = "0"
    if upstream != "(no upstream set)":
        rc3, counts = sh("cd %s && git rev-list --left-right --count @{u}...HEAD" % REPO)
        parts = counts.split()
        if rc3 == 0 and len(parts) == 2:
            behind, ahead = parts[0], parts[1]

    print("REMOTE|%s|%s|%s|%s|%s" % (url.strip(), br.strip(), upstream, ahead, behind))
    print("")
    print("Push URL      : %s" % url.strip())
    print("Local branch  : %s" % br.strip())
    print("Tracking      : %s" % upstream)
    print("Unpushed      : %s commit(s)" % ahead)
    print("Unpulled      : %s commit(s)" % behind)
    rc, out = sh("cd %s && git remote -v" % REPO)
    print("\n--- all remotes ---\n" + out.strip())


# ------------------------------------------------------------------------- main
ACTIONS = """SWGReturn server control -- actions:
  status              machine-readable state for the GUI
  start [--reloadstrings]
  shutdown            save the world, then stop cleanly
  restart
  rebuild             cmake + make with the required flags
  console             attach to the live console (interactive)
  console_snapshot    read-only snapshot of the console
  log_tail [N]        last N lines of core3.log (default 80)
  log_errors          error-class lines + counts
  accounts            list accounts and character count
  set_admin USER [LEVEL]
  console_errors      stdout-only errors Core3 never writes to core3.log
  backup_count        integer: completed world saves seen in core3.log
  db_backup           gzip mysqldump into D:\\SWGGenesis\\backups
  target_local        point the client at THIS server (writes client cfgs + galaxy)
  target_live         point the client back at THEIR live server
  show_target         READ-ONLY: which server the client currently points at
  fix_ip / show_ip    aliases for target_local / show_target
  ports               what is listening
  planets_get         TYPE|zone|0or1 for every known zone
  planets_set --ground a,b --space c,d
  tre_check           READ-ONLY: is companion_patch.tre first, has the list drifted
  tre_sync            regenerate the TRE load order, patch first (run after a pull)
  remote              where a push would go
  diff / pull / push [--msgfile PATH]
"""


def _flag(args, name, default=""):
    if name in args:
        i = args.index(name)
        if i + 1 < len(args):
            return args[i + 1]
    return default


def main():
    args = sys.argv[1:]
    if not args:
        print(ACTIONS)
        return
    a = args[0]

    if a == "status":              act_status()
    elif a == "start":             act_start("--reloadstrings" in args)
    elif a == "shutdown":          act_shutdown()
    elif a == "restart":           act_restart()
    elif a == "rebuild":           act_rebuild()
    elif a == "console":           act_console()
    elif a == "console_snapshot":  act_console_snapshot()
    elif a == "log_tail":          act_log_tail(int(args[1]) if len(args) > 1 else 80)
    elif a == "log_errors":        act_log_errors()
    elif a == "console_errors":    act_console_errors()
    elif a == "accounts":          act_accounts()
    elif a == "set_admin":
        if len(args) < 2:
            print("Usage: set_admin USERNAME [LEVEL]")
        else:
            act_set_admin(args[1], args[2] if len(args) > 2 else "15")
    elif a == "backup_count":      act_backup_count()
    elif a == "db_backup":         act_db_backup()
    elif a == "fix_ip":            act_fix_ip()
    elif a == "show_ip":           act_show_ip()
    elif a == "target_local":      act_target_local()
    elif a == "target_live":       act_target_live()
    elif a == "show_target":       act_show_target()
    elif a == "ports":             act_ports()
    elif a == "planets_get":       act_planets_get()
    elif a == "planets_set":       act_planets_set(_flag(args, "--ground"), _flag(args, "--space"))
    elif a == "tre_check":         act_tre_check()
    elif a == "tre_sync":          act_tre_sync()
    elif a == "remote":            act_remote()
    elif a == "diff":              act_diff()
    elif a == "pull":              act_pull()
    elif a == "push":
        mf = None
        if "--msgfile" in args:
            i = args.index("--msgfile")
            if i + 1 < len(args):
                mf = args[i + 1]
        act_push(mf)
    else:
        print("Unknown action: %s\n\n%s" % (a, ACTIONS))


if __name__ == "__main__":
    main()

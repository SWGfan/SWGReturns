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
import shutil
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
ACTIVITY_LOG = "/mnt/d/SWGGenesis/menu_activity.log"  # 2026-08-07: every real action, tee'd live -- see act_rebuild()/main()
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


ACTIVITY_LOG_MAX_LINES = 4000  # trimmed on each open so the GUI's tail never loads an unbounded file

# High-frequency, read-only actions the GUI itself polls every few seconds
# (status/console/players/...) -- excluded from the activity log, or every
# real action (a rebuild, a push, a restart) would scroll off within seconds
# of the panel just sitting open.
_NOISY_READONLY_ACTIONS = {
    "status", "console_live", "console_snapshot", "console_errors",
    "log_tail", "log_errors", "players", "accounts", "backup_count",
    "remote", "planets_get", "tune_get", "tune_file", "show_target",
    "show_ip", "ports", "diff",
}

# Set by main() for the life of one action; module-level so deep helpers like
# sh_stream() can tee into it without every act_* function needing a
# parameter that exists only for this.
_current_activity_log = None


class _Tee:
    """Mirrors everything written to one stream into several. Used to send
    every print() in an action to both real stdout (so a CLI/WSL caller still
    sees it) and the shared activity log (so the GUI's Activity Log pane
    shows it too) -- live, per line, not just once the action finishes.
    """
    def __init__(self, *streams):
        self.streams = streams

    def write(self, data):
        for s in self.streams:
            s.write(data)
            s.flush()

    def flush(self):
        for s in self.streams:
            s.flush()


def _open_activity_log(action, rest_args):
    """2026-08-07: open the shared activity log for append and write a start
    banner, so ANY invocation of this script -- button or terminal -- shows
    up in the control panel's Activity Log pane. Trims the file to its last
    ACTIVITY_LOG_MAX_LINES first so it never grows without bound. Never
    raises -- a logging problem must never block the real action.
    """
    try:
        os.makedirs(os.path.dirname(ACTIVITY_LOG) or ".", exist_ok=True)
        if os.path.exists(ACTIVITY_LOG):
            with open(ACTIVITY_LOG, "r", encoding="utf-8", errors="replace") as f:
                lines = f.readlines()
            if len(lines) > ACTIVITY_LOG_MAX_LINES:
                with open(ACTIVITY_LOG, "w", encoding="utf-8") as f:
                    f.writelines(lines[-ACTIVITY_LOG_MAX_LINES:])

        f = open(ACTIVITY_LOG, "a", encoding="utf-8")
        stamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        f.write("\n" + "=" * 78 + "\n")
        f.write("[%s] $ swggenesis_menu.py %s\n" % (stamp, " ".join([action] + list(rest_args))))
        f.write("-" * 78 + "\n")
        f.flush()
        return f
    except OSError:
        return None


def sh_stream(cmd, timeout=None, tee_file=None, echo=True):
    """Like sh(), but reads output line by line as it is produced instead of
    capturing silently until the process exits. If tee_file is given, every
    line is ALSO written there immediately (flushed) -- used so the shared
    activity log gets full, live detail (e.g. every file make compiles) even
    when the caller only prints a short summary to stdout/the GUI's own
    captured output. Pass echo=False to suppress per-line stdout printing
    entirely (tee_file-only, for very verbose steps like `make`) -- the
    caller is still free to print its own summary from the returned text.

    Returns (rc, full combined output), same shape as sh().
    """
    start = time.time()
    lines = []
    try:
        p = subprocess.Popen(cmd, shell=True, executable="/bin/bash",
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                             bufsize=1, universal_newlines=True)
    except OSError as e:
        return 1, "(failed to start: %s)" % e

    def _emit(text):
        if echo:
            print(text, end="")
        if tee_file is not None:
            try:
                tee_file.write(text)
                tee_file.flush()
            except OSError:
                pass

    try:
        for line in p.stdout:
            lines.append(line)
            _emit(line)
            if timeout is not None and (time.time() - start) > timeout:
                p.kill()
                extra = "(timed out after %ss)\n" % timeout
                lines.append(extra)
                _emit(extra)
                break
    finally:
        p.stdout.close()
        rc = p.wait()

    return rc, "".join(lines)


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


def snapshot(tail=0):
    """Read the server console's current screen + scrollback.

    tail > 0 returns only the last N non-blank lines, which is what a live view
    wants: the whole scrollback is tens of thousands of lines and re-sending it
    every few seconds is what made the GUI's console pane sluggish and always
    scrolled to the wrong place."""
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
    lines = [l.rstrip() for l in data.splitlines() if l.strip()]
    if tail > 0:
        lines = lines[-tail:]
    return "\n".join(lines)


# screen wraps every line at the window's column count, and a detached
# `screen -dmS` defaults to 80. That is why console output arrives broken
# mid-word at ~78 characters and is painful to read in the GUI. Widening the
# window makes subsequent output wrap at WIDE_COLS instead. It does not
# retroactively fix lines already in the scrollback.
WIDE_COLS = 200


def set_console_width(cols=WIDE_COLS):
    sh("screen -S %s -p 0 -X width %d" % (SCREEN, cols))


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
        # GENESIS_TIMING_2026_08_04 -- first sighting of a fully-open server
        # closes the boot timer that act_start() opened.
        if LOGIN_PORT in ports:
            timing_mark_done("boot")
        extra = "pid %s, listening on %s" % (pid, ", ".join(str(p) for p in ports))
        if LOGIN_PORT not in ports:
            print("SESSION_NO_PROC|pid %s but login port %d is NOT listening "
                  "(startup incomplete or an exception aborted init)" % (pid, LOGIN_PORT))
            return
        print("RUNNING|" + extra)
    elif pid and not ports:
        print("SESSION_NO_PROC|pid %s alive but no ports listening yet -- still booting "
              "-- %s" % (pid, timing_phrase("boot")))
    elif sess and not pid:
        print("SESSION_NO_PROC|screen session '%s' exists but no core3 process "
              "(likely crashed -- View Live Console to see the gdb prompt)" % SCREEN)
    else:
        print("STOPPED|no core3 process, no screen session")

    # BUILD_PROGRESS_2026_08_04 -- a second line so the panel can tell a
    # compiling server from a dead one. Previously both read "GENESIS STOPPED"
    # and a long build was indistinguishable from a wedged job.
    build = _build_progress_line()

    if build:
        print(build)


# GENESIS_TIMING_2026_08_04 -- measure how long builds and boots really take on
# this machine and show a countdown, rather than hardcoding a number that is
# wrong the moment a header change turns a 2-file rebuild into a 200-file one.
TIMING_MARKS = REPO + "/.genesis_timing_marks"
TIMING_LOG   = REPO + "/.genesis_timings"
TIMING_KEEP  = 20


def _timing_marks():
    marks = {}
    try:
        with open(TIMING_MARKS) as fh:
            for line in fh:
                if "=" in line:
                    k, v = line.strip().split("=", 1)
                    try:
                        marks[k] = float(v)
                    except ValueError:
                        pass
    except OSError:
        pass
    return marks


def _timing_write_marks(marks):
    try:
        with open(TIMING_MARKS, "w") as fh:
            for k, v in marks.items():
                fh.write("%s=%f\n" % (k, v))
    except OSError:
        pass


def timing_mark_start(kind):
    """Record that `kind` just began, unless it is already running."""
    marks = _timing_marks()
    if kind not in marks:
        marks[kind] = time.time()
        _timing_write_marks(marks)


def timing_mark_done(kind):
    """Close out `kind` and append its duration. Returns the duration, or None."""
    marks = _timing_marks()
    started = marks.pop(kind, None)
    _timing_write_marks(marks)

    if started is None:
        return None

    elapsed = time.time() - started

    # A 3-second "build" is a no-op make, not a data point worth learning from.
    if elapsed < 20 or elapsed > 4 * 3600:
        return elapsed

    try:
        rows = []
        if os.path.exists(TIMING_LOG):
            rows = [l.strip() for l in open(TIMING_LOG) if l.strip()]
        rows.append("%s %d" % (kind, int(elapsed)))

        kept, seen = [], {}
        for row in reversed(rows):
            k = row.split()[0]
            seen[k] = seen.get(k, 0) + 1
            if seen[k] <= TIMING_KEEP:
                kept.append(row)

        with open(TIMING_LOG, "w") as fh:
            fh.write("\n".join(reversed(kept)) + "\n")
    except (OSError, IndexError):
        pass

    return elapsed


def timing_typical(kind):
    """Median of recent durations for `kind`, or None with no history.

    Median rather than mean: one 40-minute full rebuild among a dozen 6-minute
    incrementals should not drag every later estimate upward.
    """
    try:
        vals = sorted(int(l.split()[1]) for l in open(TIMING_LOG)
                      if l.strip().startswith(kind + " "))
    except (OSError, ValueError, IndexError):
        return None

    if not vals:
        return None

    mid = len(vals) // 2
    return vals[mid] if len(vals) % 2 else (vals[mid - 1] + vals[mid]) // 2


def timing_eta(kind):
    """(elapsed, typical, remaining) -- remaining is negative when overdue."""
    marks = _timing_marks()
    started = marks.get(kind)

    if started is None:
        return None, timing_typical(kind), None

    elapsed = int(time.time() - started)
    typical = timing_typical(kind)

    if typical is None:
        return elapsed, None, None

    return elapsed, typical, typical - elapsed


def timing_phrase(kind):
    """Human tail for the status line."""
    elapsed, typical, remaining = timing_eta(kind)

    if typical is None:
        return "no estimate yet (first run)"

    if remaining is None:
        return "usually %s" % fmt_secs(typical)

    if remaining > 0:
        return "~%s left (usually %s)" % (fmt_secs(remaining), fmt_secs(typical))

    # Past the estimate. Say so plainly -- twice tonight a wedged job looked
    # exactly like a healthy one, and "overdue" is the cheapest possible warning.
    return "OVERDUE by %s (usually %s)" % (fmt_secs(-remaining), fmt_secs(typical))


def fmt_secs(s):
    s = int(s)
    if s < 60:
        return "%ds" % s
    if s < 3600:
        return "%dm %02ds" % (s // 60, s % 60)
    return "%dh %02dm" % (s // 3600, (s % 3600) // 60)


def _build_progress_line():
    """One BUILD|... line for the GUI, or None when nothing is building.

    Deliberately cheap: this runs on every `status` call, which the control
    panel polls every 5 seconds.
    """
    import glob as _glob

    logs = _glob.glob("/tmp/build*.log") + _glob.glob("/tmp/genesis_build.log")

    newest, newest_age = None, None
    for p in logs:
        try:
            age = time.time() - os.path.getmtime(p)
        except OSError:
            continue
        if newest_age is None or age < newest_age:
            newest, newest_age = p, age

    # A live compiler is proof on its own; the log's mtime covers the gaps where
    # make is grinding through one big translation unit and printing nothing.
    rc, _ = sh("pgrep -x cc1plus >/dev/null 2>&1 && echo y")
    compiling = rc == 0

    if newest is None:
        if compiling:
            timing_mark_start("build")
            return "BUILD|BUILDING|0|starting|" + timing_phrase("build")
        return None

    fresh = newest_age is not None and newest_age < 180

    if not fresh and not compiling:
        # Nothing running. Mention a recent build briefly so a finished GUI
        # rebuild does not just vanish without a word.
        if newest_age is not None and newest_age < 900:
            rc, tail = sh("tail -3 '%s' 2>/dev/null" % newest)
            state = "DONE" if "Built target core3" in tail else "STOPPED"
            took = timing_mark_done("build")
            detail = "%d min ago" % int(newest_age // 60)
            if took:
                detail += ", took " + fmt_secs(took)
            return "BUILD|%s|100|%s|" % (state, detail)
        return None

    rc, tail = sh("tail -40 '%s' 2>/dev/null" % newest)

    pct, detail, state = 0, "compiling", "BUILDING"

    for ln in reversed(tail.splitlines()):
        ln = ln.strip()
        if not ln.startswith("["):
            continue
        close = ln.find("]")
        if close < 0:
            continue
        try:
            pct = int(ln[1:close].strip().rstrip("%"))
        except ValueError:
            continue
        rest = ln[close + 1:].strip()
        if "Linking" in rest:
            state, detail = "LINKING", "core3"
        elif "Built target core3" in rest:
            state, detail = "DONE", "core3"
        elif "Building" in rest:
            detail = rest.rsplit("/", 1)[-1].replace(".o", "")
        else:
            detail = rest[:60]
        break

    timing_mark_start("build")

    if state == "DONE":
        took = timing_mark_done("build")
        if took:
            detail += " (took %s)" % fmt_secs(took)
        return "BUILD|DONE|100|%s|" % detail

    return "BUILD|%s|%d|%s|%s" % (state, pct, detail, timing_phrase("build"))


def act_start(reloadstrings=False):
    # GENESIS_TIMING_2026_08_04 -- start the boot clock; the first RUNNING status
    # closes it and records how long this machine actually takes.
    timing_mark_start("boot")

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
    set_console_width()          # before any real output, so nothing wraps at 80
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
    rc, out = sh_stream("cd %s && cmake %s ../.. 2>&1" % (BUILD, " ".join(CMAKE_FLAGS)),
                        timeout=900, tee_file=_current_activity_log, echo=False)
    print("\n".join(out.strip().splitlines()[-5:]))
    if rc != 0:
        print("\nCMAKE FAILED -- stopping here.")
        return

    print("\nCompiling (make -j4). A full rebuild takes about an hour on the D: drive -- "
          "watch the control panel's Activity Log pane (or `tail -f menu_activity.log`) "
          "for live per-file progress...")
    rc, out = sh_stream("cd %s && make -j4 2>&1" % BUILD, timeout=14400,
                        tee_file=_current_activity_log, echo=False)
    print("\n".join(out.strip().splitlines()[-25:]))
    if rc == 0:
        print("\n=== BUILD OK ===")
    else:
        print("\n=== BUILD FAILED -- first error below ===")
        rc2, err = sh("cd %s && make -j4 2>&1 | grep -m1 -B4 -A15 'error:'" % BUILD, timeout=600)
        print(err.strip() or "(no 'error:' line found -- see output above)")


def act_console_snapshot():
    s = snapshot()
    print(s if s.strip() else "(no console output -- server not running?)")


def act_console_live(n=60):
    """Last N console lines. What the GUI's live pane polls -- cheap enough to
    call every couple of seconds, unlike the full scrollback."""
    s = snapshot(tail=n)
    print(s if s.strip() else "(no console output -- server not running?)")


def act_console_width(cols=WIDE_COLS):
    """Widen the running console so new output stops wrapping at 80 columns."""
    if not screen_exists():
        print("No '%s' screen session -- start the server first." % SCREEN)
        return
    set_console_width(cols)
    print("Console window width set to %d columns." % cols)
    print("Affects NEW output only; lines already in the scrollback stay wrapped.")


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


def _backup_prune(keep=6):
    """Keep only the newest `keep` backup folders."""
    try:
        entries = [d for d in os.listdir(BACKUPS)
                   if os.path.isdir(os.path.join(BACKUPS, d))]
    except OSError:
        return
    entries.sort(reverse=True)          # names are timestamps -> newest first
    for old in entries[keep:]:
        path = os.path.join(BACKUPS, old)
        print("  pruning old backup %s" % old)
        shutil.rmtree(path, ignore_errors=True)


RESTORE_NOTE = """SWGGenesis backup -- taken %s

CONTENTS
  databases/       the object database: every character, companion, item,
                   structure and waypoint. Berkeley DB.
  genesis.sql.gz   MySQL: accounts, galaxy metadata, schema version.

__db.001/002/003 are DELIBERATELY NOT INCLUDED. Those are Berkeley DB
environment region files -- shared-memory scratch that the engine recreates on
every startup. Restoring stale ones alongside good data can make the engine
open an environment that disagrees with the databases it points at. The real
data is the .db files plus log.*, which is what you see here.

TO RESTORE
  1. Stop the server:
       python3 /mnt/d/SWGGenesis/swggenesis_menu.py shutdown

  2. Move the current object database aside (do NOT delete it until the
     restore is proven good):
       mv /mnt/d/SWGGenesis/MMOCoreORB/bin/databases \
          /mnt/d/SWGGenesis/MMOCoreORB/bin/databases.before-restore

  3. Put this one back:
       cp -a %s/databases /mnt/d/SWGGenesis/MMOCoreORB/bin/databases

  4. MySQL:
       gunzip -c %s/genesis.sql.gz | mysql -u %s -p%s %s

  5. Start, and log in to check a character before deleting the folder from
     step 2:
       python3 /mnt/d/SWGGenesis/swggenesis_menu.py start
"""


def act_backup_all(stop_first=False, mysql_only=False, auto=False, keep=6,
                   restart_after=False):
    """Back up BOTH halves of the server state. See the module docstring."""
    # BACKUP_RESTART_AFTER_2026_08_05 -- only restart a server we stopped ourselves.
    # If it was already down when the backup started, it stays down: taking a
    # backup is not a reason to boot a server nobody asked to run.
    we_stopped_it = False

    if stop_first and core3_pid():
        print("Server is up -- saving the world and shutting down first.")
        act_shutdown()
        for _ in range(40):
            if not core3_pid():
                break
            time.sleep(5)
        we_stopped_it = not core3_pid()

    running = core3_pid()

    # AUTO_MODE_2026_08_04 -- unattended runs must never do the wrong thing.
    # Stopped -> full backup. Running -> MySQL only, stated plainly, rather
    # than either producing nothing (plain backup_all refuses) or shutting the
    # server down under whoever is online (--stop-first).
    if auto and running:
        print("AUTO: server is UP (pid %s) -- backing up MySQL only." % running)
        print("      The object database (characters, companions, items) needs")
        print("      the server stopped to copy safely, so it is SKIPPED tonight.")
        print("      Shut the server down before the scheduled time, or press")
        print("      Backup Everything by hand, to capture it.")
        print("")
        mysql_only = True
    elif auto:
        print("AUTO: server is stopped -- taking a full backup.")
        print("")

    if running and not mysql_only:
        print("REFUSED: the server is running (pid %s)." % running)
        print("")
        print("A Berkeley DB copied while it is being written is not reliably")
        print("restorable -- you would get a torn snapshot that may or may not")
        print("open. Rather than hand you a backup that might not work, pick one:")
        print("")
        print("  backup_all --stop-first    save, shut down, back up, stay down")
        print("  backup_all --stop-first --restart-after")
        print("                             the same, then start it again")
        print("  backup_all --mysql-only    skip the object DB, dump MySQL only")
        return 1

    os.makedirs(BACKUPS, exist_ok=True)
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    dest = os.path.join(BACKUPS, stamp)
    os.makedirs(dest, exist_ok=True)
    print("Backing up into %s" % dest)
    print("")

    ok = True

    if not mysql_only:
        print("1/2  object database (excluding __db.* region files) ...")
        rc, out = sh("mkdir -p '%s/databases' && cd '%s/databases' && "
                     "tar -cf - --exclude='__db.*' . | (cd '%s/databases' && tar -xf -)"
                     % (dest, BIN, dest), timeout=3600)
        if rc != 0:
            print("     FAILED:\n" + out.strip())
            ok = False
        else:
            rc, sz = sh("du -sh '%s/databases' | cut -f1" % dest)
            rc2, n = sh("ls -1 '%s/databases'/*.db 2>/dev/null | wc -l" % dest)
            print("     wrote %s (%s .db files)" % (sz.strip(), n.strip()))
    else:
        print("1/2  object database SKIPPED (--mysql-only)")

    print("2/2  MySQL ...")
    sqlgz = os.path.join(dest, "%s.sql.gz" % DB_NAME)
    rc, out = sh("mysqldump -u %s -p%s --single-transaction --routines %s | gzip > '%s'"
                 % (DB_USER, DB_PASS, DB_NAME, sqlgz), timeout=1800)
    size = os.path.getsize(sqlgz) if os.path.exists(sqlgz) else 0

    # A failed mysqldump still produces a valid, EMPTY gzip (~20 bytes) -- which
    # is exactly how a bad-credentials backup went unnoticed on 2026-08-04.
    # Treat anything that small as a failure, loudly.
    if rc != 0 or size < 1024:
        print("     FAILED -- %d bytes. An empty gzip means the dump produced" % size)
        print("     nothing, which is almost always wrong credentials.")
        print("     " + out.strip()[:400])
        ok = False
    else:
        rc, sz = sh("du -h '%s' | cut -f1" % sqlgz)
        print("     wrote %s" % sz.strip())

    with open(os.path.join(dest, "RESTORE.txt"), "w") as fh:
        fh.write(RESTORE_NOTE % (stamp, dest, dest, DB_USER, DB_PASS, DB_NAME))

    print("")
    _backup_prune(keep)

    rc, total = sh("du -sh '%s' | cut -f1" % dest)
    rc, free = sh("df -h '%s' | tail -1 | awk '{print $4}'" % BACKUPS)
    print("")
    print("Backup %s: %s   (%s free on the drive)"
          % ("COMPLETE" if ok else "INCOMPLETE -- see errors above", total.strip(), free.strip()))
    print("Restore instructions are in %s/RESTORE.txt" % dest)
    print("")
    print("NOTE: backups live on the SAME physical disk as the server. That")
    print("      protects you from a bad patch or a corrupted save -- it does")
    print("      NOT protect you from this drive failing. Copy a folder to")
    print("      another drive periodically.")
    print("")
    rc, out = sh("ls -1t '%s' | head -8" % BACKUPS)
    print("Backups on disk (newest first):\n" + out.strip())

    # BACKUP_RESTART_AFTER_2026_08_05 -- put it back the way we found it.
    # Deliberately restarts even when the backup reported problems: a bad backup
    # is something to go and look at, but a server silently left offline is
    # something that costs players. The return code below still reports the
    # backup honestly, so nothing downstream is misled.
    if restart_after and we_stopped_it:
        print("")
        print("Backup finished -- starting the server again.")
        act_start()

    return 0 if ok else 1


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


def act_remote_set(url, force=False):
    """Point pushes at a different GitHub repo, persistently.

    The default lives in git's own config (the `origin` remote URL) rather than
    a side file, because that is already what act_push() uses and what
    `git remote -v` reports -- one source of truth instead of two that can
    drift apart.
    """
    url = (url or "").strip()

    if not url:
        print("Usage: remote_set --url https://github.com/<owner>/<repo>.git")
        return 1

    if not (url.startswith("https://") or url.startswith("git@") or url.startswith("ssh://")):
        print("That does not look like a git remote URL: %s" % url)
        print("Expected something like:")
        print("  https://github.com/SWGfan/SWGReturns.git")
        print("  git@github.com:SWGfan/SWGReturns.git")
        return 1

    # The project's standing rule, in the user's own words: "lets not upload to
    # their github directly, we should only use swgfan github and if the other
    # server wants it, they can pull my edits." A speed bump, not a lock.
    if "bfitzgit23" in url.lower() and not force:
        print("REFUSED: that is the other server's upstream repo (bfitzgit23).")
        print("")
        print("Your standing rule for this project is to push only to SWGfan and")
        print("let them pull your edits, and the panel's GitHub box says the same.")
        print("If you really mean it, re-run with --force.")
        return 1

    rc, old = sh("cd %s && git remote get-url origin 2>/dev/null" % REPO)
    had_origin = rc == 0 and old.strip()

    if had_origin:
        rc, out = sh("cd %s && git remote set-url origin '%s'" % (REPO, url))
    else:
        rc, out = sh("cd %s && git remote add origin '%s'" % (REPO, url))

    if rc != 0:
        print("FAILED to set the remote:\n" + out.strip())
        return 1

    print("Push target updated.")
    if had_origin:
        print("  was : %s" % old.strip())
    print("  now : %s" % url)
    print("")
    print("Saved in git's own config, so it is the default from here on and")
    print("survives reboots and pulls. Change it any time from the panel or with")
    print("  remote_set --url <other-url>")
    print("")

    rc, out = sh("cd %s && git ls-remote --heads '%s' 2>&1 | head -3" % (REPO, url))
    if rc == 0 and out.strip():
        print("Reachable -- remote branches found:\n" + out.strip())
    else:
        print("NOTE: could not list branches on that remote yet. That is fine if")
        print("      the repo is new or empty; it also shows up when credentials")
        print("      are not cached yet. The first push will prompt for them.")
        if out.strip():
            print("      git said: " + out.strip()[:200])
    return 0


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


# ------------------------------------------------------------------ TRE publishing
# The archive players actually download lives at the repo root in tre/, not
# buried in docs/companion_system/tools/. GitHub renders a .tre as "not
# displayed", so a generated manifest sits beside it -- that is the only way to
# see what is inside one without downloading it.
#
# This is a COPY step, deliberately. The build pipeline
# (build_companion_content.py -> build_tre_patch.py) keeps writing to tools/ as
# it always has; publishing is a separate, explicit action so a half-finished
# rebuild never lands in front of players.
TRE_BUILT = REPO + "/docs/companion_system/tools/companion_patch.tre"
TRE_PUB_DIR = REPO + "/tre"
TRE_TOOLS = REPO + "/docs/companion_system/tools"


def _tre_records(path):
    """[(path, bytes, checksum)] from an archive, using the project's own reader."""
    import importlib
    sys.path.insert(0, TRE_TOOLS)
    try:
        tre_reader = importlib.import_module("tre_reader")
        arc = tre_reader.TreArchive(path)
    except Exception as e:
        return None, str(e)
    out = []
    for rec in arc.records:
        p = rec.path
        if isinstance(p, bytes):
            p = p.decode("utf-8", "replace")
        p = (p or "").replace("\\", "/").strip("\x00").strip()
        out.append((p, rec.uncompressedSize, rec.checksum))
    out.sort(key=lambda r: r[0])
    return out, None


def act_tre_publish():
    """Copy the built TRE to <repo>/tre/ with a manifest and a README."""
    if not os.path.exists(TRE_BUILT):
        print("REFUSED: %s does not exist." % TRE_BUILT)
        print("Build it first, from inside docs/companion_system/tools/:")
        print("    python3 build_companion_content.py && python3 build_tre_patch.py")
        return

    size = os.path.getsize(TRE_BUILT)
    recs, err = _tre_records(TRE_BUILT)
    if recs is None:
        # No manifest is better than a wrong one -- but the archive itself is
        # still worth publishing, so this is a warning, not a refusal.
        print("WARNING: could not read the archive for a manifest (%s)." % err)
        print("         Publishing the .tre without one.")

    os.makedirs(TRE_PUB_DIR, exist_ok=True)
    dst = os.path.join(TRE_PUB_DIR, "companion_patch.tre")

    same = os.path.exists(dst) and os.path.getsize(dst) == size
    with open(TRE_BUILT, "rb") as f:
        data = f.read()
    if same:
        with open(dst, "rb") as f:
            same = f.read() == data
    if same:
        print("tre/companion_patch.tre is already identical -- archive not rewritten.")
    else:
        with open(dst, "wb") as f:
            f.write(data)
        print("Published tre/companion_patch.tre  (%s bytes)" % format(size, ","))

    stamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    if recs:
        lines = [
            "companion_patch.tre -- contents",
            "=" * 62,
            "",
            "Generated %s by `swggenesis_menu.py tre_publish`." % stamp,
            "Archive: %s bytes, %d records." % (format(size, ","), len(recs)),
            "",
            "A .tre is a binary archive, so GitHub shows it as \"not displayed\".",
            "This manifest exists so you can see what is inside without downloading.",
            "",
            "%-58s %12s  %s" % ("path", "bytes", "checksum"),
            "%-58s %12s  %s" % ("-" * 58, "-" * 12, "-" * 10),
        ]
        for p, n, c in recs:
            lines.append("%-58s %12s  0x%08x" % (p, format(n, ","), c))
        lines.append("")
        with open(os.path.join(TRE_PUB_DIR, "companion_patch.manifest.txt"),
                  "w", encoding="utf-8") as f:
            f.write("\n".join(lines))
        print("Wrote tre/companion_patch.manifest.txt  (%d records)" % len(recs))

    readme = README_TRE % {"stamp": stamp, "size": format(size, ","),
                           "count": len(recs) if recs else 0}
    with open(os.path.join(TRE_PUB_DIR, "README.md"), "w", encoding="utf-8") as f:
        f.write(readme)
    print("Wrote tre/README.md")
    print("")
    print("Not committed. Use Confirm && Push (or `push`) to send it to SWGfan.")


README_TRE = """# Companion client patch

`companion_patch.tre` — the one custom client archive this server uses.
**%(size)s bytes, %(count)d records.** Generated %(stamp)s.

See `companion_patch.manifest.txt` for exactly what is inside; GitHub cannot
render a `.tre`, so the manifest is the only way to inspect it without
downloading.

## What it does

Adds the Companion Handler profession's client-side content: skill tables,
command definitions, string tables, the companion control device template, and
the icon styles its commands use.

## Installing it (players)

1. Put `companion_patch.tre` in your SWG client folder, beside the other
   `.tre` files.
2. Open `swgemu_live.cfg`, find the `[SharedFile]` section, and add a line:

   ```
   searchTree_00_29=companion_patch.tre
   ```

3. **Fully close and relaunch the client.** It caches TRE contents, so
   reconnecting is not enough.

### The three things that go wrong

**Priority.** The client loads the archive with the HIGHEST `searchTree_XX_NN`
number first, and the first archive holding a path wins. This patch must be
above every archive it overrides. Pick a number higher than any already in the
file — and check `maxSearchPriority` at the top of `[SharedFile]`: an entry
above that cap is **silently ignored**, the game loads normally, and nothing
anywhere logs a reason.

**Content packs.** If your client has a content pack (aftermath, etc.), this
patch is built against it. Installing it over a *different* pack will revert
that pack's version of any file listed in the manifest.

**Loose files beat archives.** <!-- LOOSE_UI_STYLES_README_2026_08_04 -->
The client reads loose files under the game folder in preference to *any* `.tre`.
If a file exists both loosely and in an archive, the loose copy wins and the
archive copy is never consulted. See the next section — on a SWG Returns install
this is not hypothetical.

## The icons need a second step

**If the companion commands show the right names but all share one generic
icon, this is why.**

SWG Returns ships a loose `ui/ui_styles.inc` in the client folder, and a loose
file beats any archive. The icon styles inside `companion_patch.tre` are
therefore never read, and every companion command falls back to the same default
glyph.

What makes this expensive to diagnose is that nothing looks wrong. The archive
loads. Its command tables and string tables work — the commands appear in the
Command Browser with their proper names, which *proves* the archive is being
read. Only the icons fail, and they fail silently.

The fix is to add the companion styles to the loose file as well:

```bash
python3 docs/companion_system/tools/patch_loose_ui_styles.py
```

It clones each companion style from that file's own base entries (so the art
matches your client build, not ours), writes a timestamped backup first, and is
safe to re-run. Then **fully relaunch the client**.

⚠️ **The launcher owns that file.** A client update can overwrite
`ui/ui_styles.inc` and take the companion styles with it. If the icons ever
revert to the generic glyph, re-run the script above — nothing is broken, the
file was simply replaced.

*How this was found, in case you hit something similar: a stock icon (`assist`)
was deliberately repointed at different pixels inside the TRE and the client
fully relaunched. It still drew its original icon — proving the client had never
read our copy of the file. Worth remembering as a technique: when every check
says the data is correct, change something you can SEE and find out whether
anything is reading it at all.*

## Installing it (server operators)

Put it in your `TrePath` and list it **first** in `TreFiles`. Order is the
whole point — the first matching archive wins, so a patch that is not first
does nothing at all.

Start once with `reloadstrings` after installing, or item names render as raw
`.iff` paths and flytext shows unresolved STF keys.

## Rebuilding

From `docs/companion_system/tools/`:

```bash
python3 build_companion_content.py
python3 build_tre_patch.py          # prints ARCHIVE VERIFIED OK
```

then publish it here with `swggenesis_menu.py tre_publish`.
"""


# ------------------------------------------------------------------ players / admin
def mysql_rows(sql, timeout=60):
    """Rows as lists of strings. -B -N gives tab-separated output with no box
    drawing and no header, which is the only form worth parsing. The password
    on the command line makes mysql warn on stderr every time, hence 2>/dev/null
    -- without it the warning becomes the first 'row'."""
    q = sql.replace('"', '\\"')
    rc, out = sh('mysql -B -N -u %s -p%s %s -e "%s" 2>/dev/null'
                 % (DB_USER, DB_PASS, DB_NAME, q), timeout)
    rows = []
    for line in out.splitlines():
        if line.strip():
            rows.append(line.split("\t"))
    return rc, rows


def act_players():
    """ACC|id|username|admin_level|active|charcount|online   then
       CHR|account_id|character_oid|name                     for the GUI.

    'online' is best-effort. Core3 has no authoritative "who is logged in"
    table; the closest signal is account_ips, where a login writes logout=0 and
    a logout writes logout=1. So the newest row per account tells us the last
    thing that happened. A server killed with SIGKILL never writes the logout
    row, which leaves an account looking online until its next login -- that is
    a known limitation, not a bug to chase.

    NOTE those rows only exist from 2026-08-04 onward: account_ips was missing
    the galaxy_id and online_count columns the code inserts, so EVERY login and
    logout insert failed before then."""
    rc, rows = mysql_rows(
        "SELECT a.account_id, a.username, a.admin_level, a.active,"
        " (SELECT COUNT(*) FROM characters c"
        "    WHERE c.account_id = a.account_id AND c.galaxy_id = %d),"
        " COALESCE((SELECT ai.logout FROM account_ips ai"
        "    WHERE ai.account_id = a.account_id"
        "    ORDER BY ai.idaccount_ips DESC LIMIT 1), 1)"
        " FROM accounts a ORDER BY a.username;" % GALAXY_ID)

    if rc != 0 or not rows:
        print("Could not read the accounts table (rc=%d)." % rc)
        return

    for r in rows:
        if len(r) < 6:
            continue
        acct, user, lvl, active, chars, lastlogout = r[0], r[1], r[2], r[3], r[4], r[5]
        online = "1" if lastlogout == "0" else "0"
        print("ACC|%s|%s|%s|%s|%s|%s" % (acct, user, lvl, active, chars, online))

    rc2, crows = mysql_rows(
        "SELECT c.account_id, c.character_oid, c.firstname,"
        " IFNULL(c.surname,'') FROM characters c"
        " WHERE c.galaxy_id = %d ORDER BY c.firstname;" % GALAXY_ID)
    for r in crows:
        if len(r) < 4:
            continue
        name = (r[2] + " " + r[3]).strip()
        print("CHR|%s|%s|%s" % (r[0], r[1], name))


# ------------------------------------------------------------------ server tuning
# Gameplay knobs the GUI can edit. Every one of these is a top-level assignment
# in bin/scripts/managers/player_manager.lua, read by the C++ at load time via
# lua->getGlobalFloat(), so a change costs a RESTART and nothing more -- no
# rebuild, no TRE.
#
# WHY A WHITELIST
# The alternative is letting the GUI hand a key and a value to a regex loose in
# the script tree. This list is the entire contract: an unknown key is refused,
# a non-numeric value is refused, and a value outside the range is refused. The
# ranges are sanity rails, not balance opinions -- they exist to stop a typo
# (60000 instead of 6) writing something that makes the server unplayable and
# is then hard to attribute.
#
# WHY THE DEFAULT IS NOT HARDCODED HERE
# "Default" means the value genesis actually ships, read from
# `git show genesis:<path>` at call time. A hardcoded table would silently rot
# the next time upstream retunes something, and the Default button would then
# quietly restore a number nobody chose.
TUNE_REL  = "MMOCoreORB/bin/scripts/managers/player_manager.lua"
TUNE_FILE = BIN + "/scripts/managers/player_manager.lua"

# key, label, kind, min, max
TUNABLES = [
    ("globalExpMultiplier",        "XP multiplier (all XP types)",     "float", 0.1,  100.0),
    ("groupExpMultiplier",         "Group XP bonus",                   "float", 1.0,   10.0),
    ("performanceBuff",            "Entertainer buff strength",        "int",     0,  20000),
    ("medicalBuff",                "Doctor buff strength",             "int",     0,  20000),
    ("performanceDuration",        "Entertainer buff duration (sec)",  "int",    60, 604800),
    ("medicalDuration",            "Doctor buff duration (sec)",       "int",    60, 604800),
    ("cheapPerformanceBuff",       "Cheap entertainer buff",           "int",     0,  20000),
    ("cheapMedicalBuff",           "Cheap doctor buff",                "int",     0,  20000),
    ("expensivePerformanceBuff",   "Expensive entertainer buff",       "int",     0,  20000),
    ("expensiveMedicalBuff",       "Expensive doctor buff",            "int",     0,  20000),
    ("onlineCharactersPerAccount", "Characters online per account",    "int",     1,     10),
    ("baseStoredVehicles",         "Stored vehicles",                  "int",     0,     20),
    ("baseStoredCreaturePets",     "Stored creature pets",             "int",     0,     20),
    ("baseStoredFactionPets",      "Stored faction pets",              "int",     0,     20),
    ("baseStoredDroids",           "Stored droids",                    "int",     0,     20),
]

TUNE_BY_KEY = {t[0]: t for t in TUNABLES}


def _tune_pattern(key):
    """An UNCOMMENTED top-level `key = <number>`.

    The (?![ \\t]*--) guard matters: player_manager.lua carries commented-out
    example values, and rewriting one of those would look like it worked while
    changing nothing the server reads."""
    return re.compile(r'^(?![ \t]*--)([ \t]*' + re.escape(key)
                      + r'[ \t]*=[ \t]*)(-?\d+(?:\.\d+)?)', re.M)


def _tune_read(text, key):
    m = _tune_pattern(key).search(text)
    return m.group(2) if m else None


def _tune_defaults():
    """The values genesis ships, straight from the branch."""
    rc, out = sh("cd %s && git show genesis:%s 2>/dev/null" % (REPO, TUNE_REL))
    return out if rc == 0 else ""


def act_tune_get():
    """TUNE|key|label|current|default|kind|min|max per line, for the GUI."""
    live = _read(TUNE_FILE)
    if not live:
        print("Could not read %s" % TUNE_FILE)
        return
    shipped = _tune_defaults()

    for key, label, kind, lo, hi in TUNABLES:
        cur = _tune_read(live, key)
        if cur is None:
            continue                       # not present on this base; just skip
        dflt = _tune_read(shipped, key) if shipped else ""
        print("TUNE|%s|%s|%s|%s|%s|%s|%s"
              % (key, label, cur, dflt if dflt is not None else "", kind, lo, hi))


def act_tune_set(key, value):
    t = TUNE_BY_KEY.get(key)
    if not t:
        print("REFUSED: '%s' is not a tunable this tool knows about." % key)
        return
    _, label, kind, lo, hi = t

    try:
        num = float(value)
    except (TypeError, ValueError):
        print("REFUSED: '%s' is not a number." % value)
        return
    if num < lo or num > hi:
        print("REFUSED: %s must be between %s and %s (got %s)." % (label, lo, hi, value))
        return

    text = _read(TUNE_FILE)
    if not text:
        print("REFUSED: could not read %s" % TUNE_FILE)
        return

    pat = _tune_pattern(key)
    hits = pat.findall(text)
    if len(hits) == 0:
        print("REFUSED: no uncommented top-level '%s = <number>' found." % key)
        return
    if len(hits) > 1:
        # Two live assignments means the last one wins at load time and editing
        # the first would change nothing observable. Refuse rather than guess.
        print("REFUSED: '%s' is assigned %d times in the file. Fix that by hand "
              "first -- editing the wrong one would silently do nothing."
              % (key, len(hits)))
        return

    old = pat.search(text).group(2)
    new_val = str(int(num)) if kind == "int" else ("%g" % num)
    if old == new_val:
        print("%s is already %s -- nothing to do." % (label, new_val))
        return

    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    bak = "%s.%s.bak" % (TUNE_FILE, stamp)
    n = 1
    while os.path.exists(bak):
        bak = "%s.%s_%d.bak" % (TUNE_FILE, stamp, n)
        n += 1
    with open(bak, "w", encoding="utf-8") as f:
        f.write(text)

    # Only the number is replaced. Indentation and any trailing `-- comment`
    # on the line are left exactly as they were.
    with open(TUNE_FILE, "w", encoding="utf-8") as f:
        f.write(pat.sub(lambda m: m.group(1) + new_val, text, count=1))

    print("%s: %s -> %s" % (label, old, new_val))
    print("Backup: %s" % bak)
    print("Takes effect on the NEXT SERVER START.")


def act_tune_file():
    """Where the tunables live, for the GUI's info line."""
    print(TUNE_FILE)


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
  console_snapshot    read-only snapshot of the console (full scrollback)
  console_live [N]    last N console lines (default 60) -- cheap, for a live view
  console_width [N]   stop the console wrapping at 80 columns (default 200)
  log_tail [N]        last N lines of core3.log (default 80)
  log_errors          error-class lines + counts
  accounts            list accounts and character count
  players             ACC|/CHR| rows for the GUI: admin level, chars, online
  set_admin USER [LEVEL]
  console_errors      stdout-only errors Core3 never writes to core3.log
  backup_count        integer: completed world saves seen in core3.log
  db_backup           gzip mysqldump into D:\\SWGGenesis\\backups (MySQL only)
  backup_all          FULL backup: object DB + MySQL + restore notes
      --stop-first    save and shut the server down first (needed for a
                      consistent object-DB copy), leaves it stopped
      --mysql-only    skip the object DB, dump MySQL only
      --auto          for scheduled runs: full backup if the server is
                      stopped, MySQL-only (with a reason) if it is up
      --keep N        how many backup folders to retain (default 6)
  target_local        point the client at THIS server (writes client cfgs + galaxy)
  target_live         point the client back at THEIR live server
  show_target         READ-ONLY: which server the client currently points at
  fix_ip / show_ip    aliases for target_local / show_target
  ports               what is listening
  planets_get         TYPE|zone|0or1 for every known zone
  planets_set --ground a,b --space c,d
  tre_check           READ-ONLY: is companion_patch.tre first, has the list drifted
  tre_sync            regenerate the TRE load order, patch first (run after a pull)
  tre_publish         copy the built TRE to <repo>/tre/ with a manifest + README
  tune_get            TUNE|key|label|current|default|kind|min|max  (gameplay knobs)
  tune_set --key K --value V     write one whitelisted knob, with a backup
  tune_file           path of the lua file the knobs live in
  remote              where a push would go
  remote_set --url U  change where pushes go, and make it the default
                      (stored as git's own origin URL). --force to allow
                      the other server's repo, which is refused otherwise
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

    global _current_activity_log
    logf = None if a in _NOISY_READONLY_ACTIONS else _open_activity_log(a, args[1:])
    real_stdout = sys.stdout
    if logf is not None:
        _current_activity_log = logf
        sys.stdout = _Tee(real_stdout, logf)

    if a == "status":              act_status()
    elif a == "start":             act_start("--reloadstrings" in args)
    elif a == "shutdown":          act_shutdown()
    elif a == "restart":           act_restart()
    elif a == "rebuild":           act_rebuild()
    elif a == "console":           act_console()
    elif a == "console_snapshot":  act_console_snapshot()
    elif a == "console_live":      act_console_live(int(args[1]) if len(args) > 1 else 60)
    elif a == "console_width":     act_console_width(int(args[1]) if len(args) > 1 else WIDE_COLS)
    elif a == "log_tail":          act_log_tail(int(args[1]) if len(args) > 1 else 80)
    elif a == "log_errors":        act_log_errors()
    elif a == "console_errors":    act_console_errors()
    elif a == "accounts":          act_accounts()
    elif a == "players":           act_players()
    elif a == "set_admin":
        if len(args) < 2:
            print("Usage: set_admin USERNAME [LEVEL]")
        else:
            act_set_admin(args[1], args[2] if len(args) > 2 else "15")
    elif a == "backup_count":      act_backup_count()
    elif a == "db_backup":         act_db_backup()
    elif a == "backup_all":
        _keep = 6
        if "--keep" in args:
            try:
                _keep = max(1, int(args[args.index("--keep") + 1]))
            except (IndexError, ValueError):
                pass
        act_backup_all(stop_first="--stop-first" in args,
                       mysql_only="--mysql-only" in args,
                       auto="--auto" in args, keep=_keep,
                       restart_after="--restart-after" in args)
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
    elif a == "tre_publish":       act_tre_publish()
    elif a == "tune_get":          act_tune_get()
    elif a == "tune_file":         act_tune_file()
    elif a == "tune_set":          act_tune_set(_flag(args, "--key"), _flag(args, "--value"))
    elif a == "remote":            act_remote()
    elif a == "remote_set":
        _url = ""
        if "--url" in args:
            try:
                _url = args[args.index("--url") + 1]
            except IndexError:
                pass
        act_remote_set(_url, force="--force" in args)
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

    if logf is not None:
        print("[%s] -- done --" % datetime.now().strftime("%Y-%m-%d %H:%M:%S"))
        # 2026-08-07 bugfix: restore stdout to the plain, still-open stream
        # BEFORE closing logf -- otherwise Python's own interpreter-exit flush
        # of sys.stdout finds it's still the Tee, calls flush() on the now-
        # closed log handle, and prints a "ValueError: I/O operation on
        # closed file" traceback after every single logged action (confirmed
        # live, 2026-08-07 -- harmless to the action itself, but noisy).
        sys.stdout = real_stdout
        _current_activity_log = None
        logf.close()


if __name__ == "__main__":
    main()

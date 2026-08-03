#Requires AutoHotkey v2.0
#SingleInstance Force

; ============================================================================
; SWG Genesis Server Control Panel  (AutoHotkey v2, native Windows GUI)
;
; 2026-08-03. Forked from SWGReturnControlPanel_2.ahk for the genesis base
; (upstream 3f445ff1f2 "swap to genesis code as base").
;
; Shells out to WSL to run /mnt/d/SWGGenesis/swggenesis_menu.py, which holds
; all the real logic. This file is only a front end.
;
; THE ONE BIG DIFFERENCE FROM THE SWGRETURN PANEL
; The client folder here is the LAUNCHER'S install, and it ships pointed at
; THEIR LIVE SERVER (74.208.80.130:44453). It is not ours alone. So there is
; no blind "Fix IP" button: you switch deliberately between Point at LOCAL and
; Point at LIVE, the panel always shows which one is active, and the backend
; backs the cfgs up before its first write.
;
; Third server on this machine. Companion owns 44xxx and the 'swgemu' screen
; session; the old SWGReturn server owns 45xxx and 'swgreturns'; genesis owns
; 46xxx and 'genesis'. Nothing here should ever touch either of the others.
; ============================================================================

WSL_DISTRO := "Debian"
WIN_USER   := "nickw"
WSL_PY     := "/mnt/d/SWGGenesis/swggenesis_menu.py"
SCREEN_SESSION := "genesis"                        ; NOT 'swgemu', NOT 'swgreturns'
CLIENT_DIR := "D:\Launcher\newreturnbenserver"     ; the launcher's install
CLIENT_EXE := CLIENT_DIR . "\SWGEmu.exe"
CommitMsgWinPath := "C:\Users\" . WIN_USER . "\AppData\Local\Temp\genesis_commit_msg.txt"
CommitMsgWslPath := "/mnt/c/Users/" . WIN_USER . "/AppData/Local/Temp/genesis_commit_msg.txt"

HELP_TEXT := "
(
SWG GENESIS SERVER CONTROL -- WHAT EACH BUTTON DOES
----------------------------------------------------------------------
WHICH SERVER AM I PLAYING?
  This client folder is shared with SWG Returns' LIVE service -- the
  launcher installed it and pointed it at their server. The line under
  the PLAY button always tells you where it currently points.

  Point at LOCAL    -- rewrite the client cfgs to your own genesis server
                       (current WSL IP, port 46453) and update the galaxy
                       row. Backs the cfgs up before its first write.
  Point at LIVE     -- put it back exactly as the launcher had it,
                       74.208.80.130:44453. Your local server is
                       unaffected; the galaxy row is deliberately NOT
                       touched, since it describes our server and means
                       nothing to theirs.
  Show Target       -- READ-ONLY. Says which server the cfgs point at and
                       what LOCAL would need. Changes nothing.

  Why the port matters as much as the address: their live login port is
  44453, which is also Companion's. A half-applied change connects you to
  the wrong server with NO error -- just an endless
  \"Connecting to the Login Server...\". Core3's login port is UDP, so
  there is nothing to refuse the connection.

PLAY
  PLAY              -- launches the client from
                       D:\Launcher\newreturnbenserver\SWGEmu.exe.
                       If it's pointed at LOCAL it warns when your server
                       isn't running. If it's pointed at LIVE it just goes.
                       Do NOT run their launcher to start the game -- it
                       rewrites the cfgs back to their server.
                       After any .tre change, FULLY close and relaunch --
                       the client caches TRE contents.

SERVER
  Start Server      -- boots the server in screen + gdb and auto-runs it.
                       Tick 'reloadstrings' ONLY after string/TRE changes.
                       Cold boot after a database wipe ~15 min, warm ~7.
                       Status stays orange until the login port opens.
  Save World Now    -- types 'save' into the console, then watches the
                       COMPLETED-SAVE COUNT in core3.log until it rises.
                       Counted from the log, not the console: the console
                       is a fixed-size buffer whose scrollback rolls, so a
                       confirmation can scroll away before we look.
  Save && Shut Down -- saves, waits for confirmation, sends 'exit', then
                       escalates to SIGTERM and SIGKILL -- aimed at THIS
                       server's pid only, never a bare pkill, because the
                       old SWGReturn server runs a binary with the same
                       name and would be killed too.
  Restart           -- Save && Shut Down followed by Start.
  Update && Rebuild -- cmake + make -j4 in MMOCoreORB/build/unix.
                       Server must be STOPPED. ~1 hour for a full build.
                       Includes -DCOMPILE_TESTS=OFF, which genesis needs:
                       its .gitignore has a '*test*' pattern that excludes
                       utils/googletest-release-1.10.0 from the repo while
                       CMakeLists.txt:191 still add_subdirectory()s it, so
                       a fresh clone cannot configure with default options.
                       Does NOT git pull and does NOT build any .tre.

LOGS
  View Live Console -- attaches to the live console. Detach with Ctrl+A
                       then D -- that does NOT stop the server.
  Show Errors       -- error-class lines from core3.log plus counts.
  Console Errors    -- errors Core3 prints ONLY to stdout and never writes
                       to core3.log: MySQL schema errors, unregistered
                       object CRCs, schematic failures. They exist nowhere
                       else, which is why Start captures the console to
                       D:\SWGGenesis\console.log.
  Show Log Tail     -- last 120 lines of core3.log.

DATABASE & ADMIN
  Backup Database   -- gzipped mysqldump of the 'genesis' database into
                       D:\SWGGenesis\backups. MySQL side only -- the object
                       store in bin/databases is a separate Berkeley DB.
  List Accounts     -- accounts, admin levels, character count.
  Set Admin         -- sets admin_level (15 = full). Usernames are stored
                       LOWERCASE.
  Show Ports        -- genesis 46xxx, plus Companion 44xxx and old
                       SWGReturn 45xxx for comparison.

PLANETS
  Planets...        -- every zone name known to config.lua, with a
                       checkbox. Writes to config-local.lua, never to
                       config.lua: bin/conf/* is gitignored, so the
                       override survives a pull from upstream. That matters
                       here -- this tree tracks upstream exactly.
                       Genesis wraps config.lua in a Core3 = { ... } table,
                       so the override is written as Core3.ZonesEnabled.
                       A timestamped backup is made first. Takes effect on
                       the NEXT SERVER START. Refuses to disable every
                       ground zone, and refuses unknown names.

GITHUB
  ⚠ PROJECT POLICY: nothing is ever pushed to bfitzgit23/Returns-EMU.
    'origin' is SWGfan; 'upstream' is theirs and is FETCH-ONLY -- its push
    URL is deliberately unresolvable so an accidental push fails instead of
    connecting. If they want a change, they pull it from SWGfan.

  Show Changes      -- previews what's changed. Nothing is sent.
  Confirm && Push   -- commits and pushes to SWGfan. The confirmation
                       dialog re-reads the target first, so you always see
                       the destination before agreeing.
  Target            -- prints the full push target into the log pane.
  Fetch Upstream    -- git fetch from them. Does NOT merge.
----------------------------------------------------------------------
)"

; ---- shell helpers -----------------------------------------------------------
; --exec bypasses the login shell; without it wsl.exe can fall back to an
; interactive shell when output is redirected, and the GUI receives a menu
; instead of a result. Every call gets its own temp file so the 5s status
; refresh can never clobber a long-running action's output.
NewTempPath() {
    global WIN_USER
    return "C:\Users\" . WIN_USER . "\AppData\Local\Temp\genesis_gui_" . A_TickCount . "_" . Random(1, 999999) . ".txt"
}

BuildCmd(actionArgs, tmpPath) {
    global WSL_DISTRO, WSL_PY
    return 'cmd.exe /c wsl.exe -d ' . WSL_DISTRO . ' --exec python3 ' . WSL_PY . ' ' . actionArgs . ' > "' . tmpPath . '" 2>&1'
}

RunCapture(actionArgs) {
    tmp := NewTempPath()
    RunWait(BuildCmd(actionArgs, tmp), , "Hide")
    out := ""
    if FileExist(tmp) {
        out := FileRead(tmp)
        try FileDelete(tmp)
    }
    return out
}

RunAsync(actionArgs, cb) {
    tmp := NewTempPath()
    Run(BuildCmd(actionArgs, tmp), , "Hide", &pid)
    fn := 0
    fn := () => (
        ProcessExist(pid) ? 0 : (SetTimer(fn, 0), FinishAsync(tmp, cb))
    )
    SetTimer(fn, 500)
}

FinishAsync(tmp, cb) {
    out := ""
    if FileExist(tmp) {
        out := FileRead(tmp)
        try FileDelete(tmp)
    }
    cb.Call(out)
}

SendConsoleCommand(cmdText) {
    global WSL_DISTRO, SCREEN_SESSION
    RunWait("cmd.exe /c wsl.exe -d " . WSL_DISTRO . " --exec bash -c `"screen -S " . SCREEN_SESSION . " -p 0 -X stuff $'" . cmdText . "\r'`"", , "Hide")
}

; ---- GUI --------------------------------------------------------------------
MyGui := Gui("+Resize", "SWG Genesis Server Control Panel")
MyGui.MarginX := 14
MyGui.MarginY := 12
MyGui.SetFont("s10", "Segoe UI")

StatusText := MyGui.Add("Text", "w980 r1 vStatusText cGreen", "Checking status...")

MyGui.Add("Text", "y+14", "")

ReloadCB := MyGui.Add("Checkbox", "vReloadCB", "reloadstrings on next start (only needed after string/TRE content changes)")

; --- play row
MyGui.Add("Text", "y+10", "")
MyGui.SetFont("s12 Bold", "Segoe UI")
BtnPlay := MyGui.Add("Button", "w280 h44", "▶   PLAY")
MyGui.SetFont("s10 Norm", "Segoe UI")
PlayHint := MyGui.Add("Text", "x+16 yp+4 w660 vPlayHint",
    "Launches D:\Launcher\newreturnbenserver\SWGEmu.exe. Don't start the game from their launcher -- it resets the cfgs.")

; Which server the client is pointed at. This folder is shared with their live
; service, so this is never assumed -- it is read from the cfgs on demand.
TargetLabel := MyGui.Add("Text", "xm y+10 w980 vTargetLabel cGray", "Client target: (checking...)")

BtnLocal   := MyGui.Add("Button", "xm y+8 w170 h32", "Point at LOCAL")
BtnLive    := MyGui.Add("Button", "x+10 w170 h32", "Point at LIVE")
BtnTarget  := MyGui.Add("Button", "x+10 w170 h32", "Show Target")

; --- server row
BtnStart    := MyGui.Add("Button", "xm y+14 w180 h32", "Start Server")
BtnSaveNow  := MyGui.Add("Button", "x+10 w180 h32", "Save World Now")
BtnShutdown := MyGui.Add("Button", "x+10 w180 h32", "Save && Shut Down")
BtnRestart  := MyGui.Add("Button", "x+10 w180 h32", "Restart")
BtnRebuild  := MyGui.Add("Button", "x+10 w180 h32", "Update && Rebuild")

; --- logs row
BtnConsole  := MyGui.Add("Button", "xm y+8 w155 h32", "View Live Console")
BtnErrors   := MyGui.Add("Button", "x+10 w155 h32", "Show Errors")
BtnConErr   := MyGui.Add("Button", "x+10 w155 h32", "Console Errors")
BtnLogTail  := MyGui.Add("Button", "x+10 w155 h32", "Show Log Tail")
BtnRefresh  := MyGui.Add("Button", "x+10 w155 h32", "Refresh Status")
BtnHelp     := MyGui.Add("Button", "x+10 w155 h32", "Help")

; --- database & admin
MyGui.Add("GroupBox", "xm y+16 w980 h80 vDbBox", "Database, Admin && Planets")
BtnBackup   := MyGui.Add("Button", "xm+16 yp+30 w150 h32", "Backup Database")
BtnAccounts := MyGui.Add("Button", "x+8 w130 h32", "List Accounts")
MyGui.Add("Text", "x+12 yp+8 w62", "Account:")
AdminEdit   := MyGui.Add("Edit", "x+4 y+-8 w150 h24 vAdminEdit", "nickwill86")
BtnSetAdmin := MyGui.Add("Button", "x+8 yp-4 w100 h32", "Set Admin")
BtnPorts    := MyGui.Add("Button", "x+8 w110 h32", "Show Ports")
BtnPlanets  := MyGui.Add("Button", "x+8 w110 h32", "Planets...")

; --- github
MyGui.Add("GroupBox", "xm y+16 w980 h112 vPushBox", "GitHub  --  pushes go to SWGfan ONLY, never to bfitzgit23")
BtnShowChanges := MyGui.Add("Button", "xm+16 yp+30 w150 h32", "Show Changes")
BtnPull        := MyGui.Add("Button", "x+10 w150 h32", "Fetch Upstream")
MyGui.Add("Text", "x+16 yp+8 w110", "Commit message:")
CommitEdit     := MyGui.Add("Edit", "x+8 y+-8 w318 h24 vCommitEdit", "Genesis updates from DESKTOP-2MI669I")
BtnConfirmPush := MyGui.Add("Button", "x+10 yp-4 w130 h32", "Confirm && Push")
BtnRemote      := MyGui.Add("Button", "x+8 w70 h32", "Target")
BtnConfirmPush.Enabled := false
RemoteLabel    := MyGui.Add("Text", "xm+16 y+14 w940 vRemoteLabel cGray", "Push target: (checking...)")

LogBox := MyGui.Add("Edit", "xm y+18 w980 h240 ReadOnly VScroll", Trim(HELP_TEXT))

MyGui.Add("Text", "xm y+14", "Live Server Console (auto-refreshes every 4s, read-only snapshot -- does not send keystrokes):")
ConsoleBox := MyGui.Add("Edit", "xm y+6 w980 h220 ReadOnly VScroll", "(checking...)")

MyGui.OnEvent("Close", (*) => ExitApp())
MyGui.Show()

SetLog(text) {
    LogBox.Value := text
}

RefreshStatus(*) {
    out := RunCapture("status")
    line := Trim(out)
    parts := StrSplit(line, "|")
    kind := parts.Length ? parts[1] : ""
    if (kind = "RUNNING") {
        StatusText.Value := "● GENESIS RUNNING   " . StrReplace(line, "RUNNING|", "")
        StatusText.Opt("cGreen")
    } else if (kind = "SESSION_NO_PROC") {
        StatusText.Value := "◐ BOOTING / PROBLEM   " . StrReplace(line, "SESSION_NO_PROC|", "")
        StatusText.Opt("cFFA500")
    } else if (kind = "STOPPED") {
        StatusText.Value := "○ GENESIS STOPPED   " . StrReplace(line, "STOPPED|", "")
        StatusText.Opt("cRed")
    } else {
        StatusText.Value := "Status unknown -- raw output: " . line
        StatusText.Opt("cRed")
    }
}

; ---- client target ----------------------------------------------------------
; Read from the cfgs, never remembered. This folder is shared with their live
; service and their launcher can rewrite it behind our back at any time.
ClientTarget := "UNKNOWN"

RefreshTarget(*) {
    global TargetLabel, ClientTarget
    out := RunCapture("show_target")
    ClientTarget := "UNKNOWN"
    Loop Parse, out, "`n", "`r" {
        l := Trim(A_LoopField)
        if (SubStr(l, 1, 22) = "Currently pointing at:") {
            v := Trim(SubStr(l, 23))
            if InStr(v, "local")
                ClientTarget := "LOCAL"
            else if InStr(v, "LIVE")
                ClientTarget := "LIVE"
            break
        }
    }
    if (ClientTarget = "LOCAL") {
        TargetLabel.Value := "Client target:  ● YOUR LOCAL GENESIS SERVER  (port 46453)      -- press 'Point at LIVE' to play their live service instead"
        TargetLabel.Opt("c008000")
    } else if (ClientTarget = "LIVE") {
        TargetLabel.Value := "Client target:  ● SWG RETURNS LIVE  (74.208.80.130:44453)      -- press 'Point at LOCAL' to play your own server"
        TargetLabel.Opt("c0000C0")
    } else {
        TargetLabel.Value := "Client target:  UNKNOWN or MIXED -- the cfgs disagree, or WSL's IP changed since they were written. Press Point at LOCAL or LIVE to settle it."
        TargetLabel.Opt("cFFA500")
    }
}

BtnTarget.OnEvent("Click", (*) => (SetLog(RunCapture("show_target")), RefreshTarget()))

BtnLocal.OnEvent("Click", PointLocal)
PointLocal(*) {
    r := MsgBox("Point the game client at YOUR LOCAL genesis server?"
              . "`n`nRewrites loginServerAddress0 and loginServerPort0 in"
              . "`n    swgemu.cfg"
              . "`n    swgemu_login.cfg"
              . "`nto the current WSL IP on port 46453, and updates the galaxy row."
              . "`n`nThe cfgs are backed up before the first write."
              . "`nYou can go back any time with 'Point at LIVE'.",
              "Point at LOCAL", "YesNo Icon?")
    if (r != "Yes")
        return
    SetLog(RunCapture("target_local"))
    RefreshTarget()
}

BtnLive.OnEvent("Click", PointLive)
PointLive(*) {
    r := MsgBox("Point the game client back at SWG RETURNS LIVE?"
              . "`n`nRestores 74.208.80.130:44453 -- exactly where the launcher"
              . "`nhad it. Your local genesis server is unaffected and keeps"
              . "`nrunning; you just won't be connecting to it.",
              "Point at LIVE", "YesNo Icon?")
    if (r != "Yes")
        return
    SetLog(RunCapture("target_live"))
    RefreshTarget()
}

SetBusy(isBusy) {
    global BtnRebuild, BtnStart, BtnShutdown, BtnSaveNow, BtnRestart, BtnBackup
    BtnRebuild.Enabled  := !isBusy
    BtnStart.Enabled    := !isBusy
    BtnShutdown.Enabled := !isBusy
    BtnSaveNow.Enabled  := !isBusy
    BtnRestart.Enabled  := !isBusy
    BtnBackup.Enabled   := !isBusy
}

ActionDone(out) {
    SetLog(out)
    SetBusy(false)
    RefreshStatus()
}

StartDone(out) {
    SetLog(out)
    SetBusy(false)
    RefreshStatus()
    SetTimer(() => RefreshStatus(), -5000)
}

BtnStart.OnEvent("Click", (*) => (
    SetBusy(true),
    SetLog("Starting genesis...`n`nIt boots in the background -- the status line above will go`norange (booting) and then green once the login port opens.`n`nCold boot after a database wipe ~15 minutes, warm boot ~7."),
    RunAsync(ReloadCB.Value ? "start --reloadstrings" : "start", StartDone)
))

BtnShutdown.OnEvent("Click", (*) => (
    SetBusy(true),
    SetLog("Saving the world, then shutting down cleanly.`n`nWaits for the save to confirm, sends 'exit', then escalates to SIGTERM`nand only then SIGKILL -- aimed at THIS server's pid, never a bare pkill,`nbecause the old SWGReturn server runs a binary with the same name."),
    RunAsync("shutdown", ActionDone)
))

BtnRestart.OnEvent("Click", (*) => (
    SetBusy(true),
    SetLog("Restarting: save, shut down, then boot again.`n`nAllow up to 20 minutes end to end."),
    RunAsync(ReloadCB.Value ? "restart --reloadstrings" : "restart", StartDone)
))

BtnRebuild.OnEvent("Click", (*) => (
    SetBusy(true),
    SetLog("Running cmake + make -j4 in MMOCoreORB/build/unix.`n`nThe window stays responsive; the result appears here when done.`nA full rebuild is about an hour on the D: drive.`n`nIncludes -DCOMPILE_TESTS=OFF, which genesis needs: its .gitignore has`na '*test*' pattern that keeps utils/googletest-release-1.10.0 out of the`nrepo while CMakeLists.txt:191 still add_subdirectory()s it.`n`nThis does NOT git pull and does NOT build any .tre."),
    RunAsync("rebuild", ActionDone)
))

; ---- PLAY -------------------------------------------------------------------
BtnPlay.OnEvent("Click", PlayClick)
PlayClick(*) {
    global CLIENT_DIR, CLIENT_EXE, ClientTarget
    if !FileExist(CLIENT_EXE) {
        MsgBox("Can't find the client:`n`n" . CLIENT_EXE, "Play", "Icon!")
        return
    }

    RefreshTarget()

    ; Only warn about our server being down when the client would actually be
    ; trying to reach it. Pointed at LIVE, our server's state is irrelevant.
    if (ClientTarget != "LIVE") {
        st := Trim(RunCapture("status"))
        if !InStr(st, "RUNNING|") {
            detail := StrReplace(StrReplace(st, "STOPPED|", ""), "SESSION_NO_PROC|", "")
            r := MsgBox("The client is pointed at your LOCAL server, but it isn't ready.`n`n"
                      . Trim(detail)
                      . "`n`nStart it first, or press 'Point at LIVE' to play their server."
                      . "`n`nLaunch the game anyway?", "Play", "YesNo Icon!")
            if (r != "Yes")
                return
        }
    }

    try {
        Run(CLIENT_EXE, CLIENT_DIR)
        SetLog("Launched " . CLIENT_EXE . "`n`n"
             . "Connecting to: " . (ClientTarget = "LIVE" ? "SWG Returns LIVE" : "your local genesis server") . "`n`n"
             . "If you just rebuilt a .tre, make sure this is a FULL relaunch --`n"
             . "the client caches TRE contents.`n`n"
             . "If it hangs on 'Connecting to the Login Server...', press`n"
             . "Show Target. Core3's login port is UDP, so a wrong address or`n"
             . "port produces no error at all -- it just waits forever.")
    } catch as e {
        MsgBox("Couldn't launch the client:`n`n" . CLIENT_EXE . "`n`n" . e.Message, "Play", "Icon!")
    }
}

BtnHelp.OnEvent("Click", (*) => SetLog(Trim(HELP_TEXT)))
BtnRefresh.OnEvent("Click", (*) => (RefreshStatus(), RefreshTarget()))
BtnErrors.OnEvent("Click", (*) => SetLog(RunCapture("log_errors")))
BtnConErr.OnEvent("Click", (*) => SetLog(RunCapture("console_errors")))
BtnLogTail.OnEvent("Click", (*) => SetLog(RunCapture("log_tail 120")))
BtnPorts.OnEvent("Click", (*) => SetLog(RunCapture("ports")))
BtnAccounts.OnEvent("Click", (*) => SetLog(RunCapture("accounts")))

BtnConsole.OnEvent("Click", (*) => (
    Run('cmd.exe /c wsl.exe -d ' . WSL_DISTRO . ' -- python3 ' . WSL_PY . ' console & echo. & echo (window closed -- press any key) & pause >nul')
))

BtnBackup.OnEvent("Click", (*) => (
    SetBusy(true),
    SetLog("Backing up the genesis database to D:\SWGGenesis\backups...`n`nPlease wait."),
    RunAsync("db_backup", ActionDone)
))

BtnSetAdmin.OnEvent("Click", SetAdminClick)
SetAdminClick(*) {
    u := Trim(AdminEdit.Value)
    if (u = "") {
        MsgBox("Type an account name first.", "Set Admin", "Icon!")
        return
    }
    r := MsgBox("Give account '" . u . "' full admin (admin_level 15) on GENESIS?", "Set Admin", "YesNo Icon?")
    if (r != "Yes")
        return
    SetLog(RunCapture("set_admin " . u . " 15"))
}

; ---- PLANETS ----------------------------------------------------------------
PlanetsGui := 0
PlanetsLV := 0
PlanetsMsg := 0

BtnPlanets.OnEvent("Click", ShowPlanets)

ShowPlanets(*) {
    global PlanetsGui, PlanetsLV, PlanetsMsg, MyGui

    if IsObject(PlanetsGui) {
        try PlanetsGui.Destroy()
    }

    PlanetsGui := Gui("+Owner" . MyGui.Hwnd, "Genesis -- Enable / Disable Planets")
    PlanetsGui.MarginX := 14
    PlanetsGui.MarginY := 12
    PlanetsGui.SetFont("s10", "Segoe UI")

    PlanetsGui.Add("Text", "w600",
        "Tick a zone to ENABLE it, untick to disable. Nothing is written until you press Apply.")
    PlanetsGui.Add("Text", "w600 cGray",
        "Written to config-local.lua as Core3.ZonesEnabled (gitignored, survives a pull), never to config.lua."
      . "`nA timestamped backup is made first. Changes take effect on the NEXT SERVER START.")

    PlanetsLV := PlanetsGui.Add("ListView", "w600 h380 Checked -Multi Grid", ["Zone", "Type"])

    BtnPReload := PlanetsGui.Add("Button", "xm y+12 w170 h32", "Reload from config")
    BtnPApply  := PlanetsGui.Add("Button", "x+10 w170 h32", "Apply Changes")
    BtnPClose  := PlanetsGui.Add("Button", "x+10 w170 h32", "Close")

    PlanetsMsg := PlanetsGui.Add("Text", "xm y+12 w600 r3 vPlanetsMsg", "")

    BtnPReload.OnEvent("Click", (*) => LoadPlanets())
    BtnPApply.OnEvent("Click", ApplyPlanets)
    BtnPClose.OnEvent("Click", (*) => PlanetsGui.Destroy())

    PlanetsGui.Show()
    LoadPlanets()
}

LoadPlanets() {
    global PlanetsLV, PlanetsMsg
    PlanetsMsg.Value := "Reading config..."
    out := RunCapture("planets_get")

    PlanetsLV.Opt("-Redraw")
    PlanetsLV.Delete()
    nGround := 0, nSpace := 0, onGround := 0, onSpace := 0

    Loop Parse, out, "`n", "`r" {
        line := Trim(A_LoopField)
        if (line = "")
            continue
        p := StrSplit(line, "|")
        if (p.Length < 3)
            continue
        isOn := (p[3] = "1")
        PlanetsLV.Add(isOn ? "Check" : "", p[2], p[1])
        if (p[1] = "GROUND") {
            nGround++
            if isOn
                onGround++
        } else {
            nSpace++
            if isOn
                onSpace++
        }
    }

    PlanetsLV.ModifyCol(1, 420)
    PlanetsLV.ModifyCol(2, 140)
    PlanetsLV.Opt("+Redraw")

    if (nGround = 0 && nSpace = 0) {
        PlanetsMsg.Value := "Couldn't read any zones. Is swggenesis_menu.py in place at /mnt/d/SWGGenesis/?`nRaw output: " . SubStr(Trim(out), 1, 200)
        return
    }

    PlanetsMsg.Value := "Ground: " . onGround . " of " . nGround . " enabled.    Space: " . onSpace . " of " . nSpace . " enabled."
}

ApplyPlanets(*) {
    global PlanetsLV, PlanetsMsg

    ground := "", space := "", nSpace := 0
    row := 0
    Loop {
        row := PlanetsLV.GetNext(row, "C")
        if !row
            break
        nm := PlanetsLV.GetText(row, 1)
        ty := PlanetsLV.GetText(row, 2)
        if (ty = "GROUND")
            ground .= (ground = "" ? "" : ",") . nm
        else {
            space .= (space = "" ? "" : ",") . nm
            nSpace++
        }
    }

    if (ground = "") {
        MsgBox("At least one GROUND zone must stay enabled -- a server with no ground zones will not boot.",
               "Planets", "Icon!")
        return
    }

    ; Genesis has no space layer at all -- 0 SpaceZone* files in the tree. The
    ; space_* names survive in config.lua as leftovers from the JTL experiment.
    if (nSpace > 0) {
        r := MsgBox("You've ticked " . nSpace . " SPACE zone(s)."
                  . "`n`nGenesis has NO space layer -- there are zero SpaceZone source"
                  . "`nfiles in the tree. These names are leftovers in config.lua from"
                  . "`nthe JTL experiment upstream reverted on 2026-08-02."
                  . "`n`nEnabling them will most likely stop the server booting. If that"
                  . "`nhappens, restore the config-local.lua backup Apply writes."
                  . "`n`nInclude them anyway?", "Planets", "YesNo Icon!")
        if (r != "Yes")
            return
    }

    if (space = "")
        space := ","

    r := MsgBox("Write these zone settings to config-local.lua?"
              . "`n`nA timestamped backup is made first."
              . "`nChanges take effect on the NEXT SERVER START.",
              "Planets", "YesNo Icon?")
    if (r != "Yes")
        return

    PlanetsMsg.Value := "Writing..."
    out := RunCapture("planets_set --ground " . ground . " --space " . space)
    SetLog(out)
    PlanetsMsg.Value := Trim(out)
    MsgBox(Trim(out), "Planets", "Iconi")
    LoadPlanets()
}

; ---- Save World Now ---------------------------------------------------------
; Types 'save' into the console, then polls the backend's completed-save COUNT
; until it goes up. From core3.log, not the console: the console is a
; fixed-size screen buffer whose scrollback rolls, so a confirmation can scroll
; away before we look. The log is append-only.
SaveBaseline := -1
SaveStartTick := 0

BackupCount() {
    out := Trim(RunCapture("backup_count"))
    if RegExMatch(out, "^\d+$")
        return Integer(out)
    return -1
}

BtnSaveNow.OnEvent("Click", StartSaveNow)

StartSaveNow(*) {
    global SaveBaseline, SaveStartTick
    st := Trim(RunCapture("status"))
    if !InStr(st, "RUNNING|") {
        MsgBox("Genesis isn't running -- there's nothing to save.", "Save World", "Icon!")
        return
    }
    SaveBaseline := BackupCount()
    if (SaveBaseline < 0) {
        MsgBox("The backend didn't return a save count.`n`nCheck that /mnt/d/SWGGenesis/swggenesis_menu.py is in place.", "Save World", "Icon!")
        return
    }
    SetBusy(true)
    SetLog("Save requested (completed saves so far: " . SaveBaseline . ").`n`nWaiting for the count to go up...`n(usually 5-10 seconds; longer on a big world)")
    SendConsoleCommand("save")
    SaveStartTick := A_TickCount
    SetTimer(PollSaveDone, 2000)
}

PollSaveDone() {
    global SaveBaseline, SaveStartTick
    now := BackupCount()
    if (now > SaveBaseline) {
        SetTimer(PollSaveDone, 0)
        SetBusy(false)
        SetLog("=== WORLD SAVED SUCCESSFULLY ===`n`nCompleted saves in core3.log: " . SaveBaseline . " → " . now . "`n`nThe MySQL side and the Berkeley DB object store are separate --`nthis saved the object store. Use Backup Database for the other.")
        TrayTip("Genesis Server", "World saved -- backup finished.", "Iconi")
        MsgBox("World saved successfully!`n`nCompleted saves went from " . SaveBaseline . " to " . now . ".", "Save World", "Iconi")
        return
    }
    if (A_TickCount - SaveStartTick > 120000) {
        SetTimer(PollSaveDone, 0)
        SetBusy(false)
        SetLog("Save requested, but the completed-save count did not increase within`n120 seconds.`n`nCheck the live console below. Two common causes:`n`n  1. The server is still BOOTING -- 'save' does nothing at all until`n     it reaches READY, and a cold boot takes ~15 minutes.`n  2. The console is sitting at a gdb prompt, i.e. the server crashed.`n`nIf the server is healthy the save probably still happened; this only`nmeans the confirmation wasn't seen in time.")
        MsgBox("Couldn't confirm the save within 120 seconds.`n`nCheck the live console -- the most likely cause is that the server`nis still booting, which makes 'save' a no-op.", "Save World", "Icon!")
    }
}

; ---- github -----------------------------------------------------------------
BtnShowChanges.OnEvent("Click", (*) => (
    BtnShowChanges.Enabled := false,
    SetLog("Reading the working tree...`n`nThis can take a while on a repo this size, especially on /mnt/d.`nThe window stays responsive."),
    RunAsync("diff", ShowChangesDone)
))

ShowChangesDone(out) {
    SetLog(out)
    BtnShowChanges.Enabled := true
    BtnConfirmPush.Enabled := true
}

BtnPull.OnEvent("Click", (*) => (
    SetLog("Fetching from upstream (NOT merging)..."),
    SetLog(RunCapture("pull")),
    RefreshRemote()
))

; ---- push target ------------------------------------------------------------
PushUrl := "", PushBranch := "", PushUpstream := "", PushAhead := "0", PushBehind := "0"

RefreshRemote(*) {
    global RemoteLabel, PushUrl, PushBranch, PushUpstream, PushAhead, PushBehind
    out := RunCapture("remote")
    PushUrl := ""

    Loop Parse, out, "`n", "`r" {
        l := Trim(A_LoopField)
        if (SubStr(l, 1, 7) = "REMOTE|") {
            p := StrSplit(l, "|")
            if (p.Length >= 6) {
                PushUrl := p[2], PushBranch := p[3], PushUpstream := p[4]
                PushAhead := p[5], PushBehind := p[6]
            }
            break
        }
    }

    if (PushUrl = "") {
        RemoteLabel.Value := "Push target: UNKNOWN -- no 'origin' remote in /mnt/d/SWGGenesis?"
        return
    }

    ; Loud if origin is anything but SWGfan. Project policy is that nothing
    ; is ever pushed to their repo, and this label is the last thing you see
    ; before pressing Push.
    warn := InStr(PushUrl, "SWGfan") ? "" : "   ⚠ NOT SWGfan -- DO NOT PUSH"
    RemoteLabel.Value := "Push target:  " . PushUrl
                       . "      branch " . PushBranch . " → " . PushUpstream
                       . "      " . PushAhead . " unpushed / " . PushBehind . " unpulled" . warn
    RemoteLabel.Opt(InStr(PushUrl, "SWGfan") ? "cGray" : "cRed")
}

BtnRemote.OnEvent("Click", (*) => (SetLog(RunCapture("remote")), RefreshRemote()))

BtnConfirmPush.OnEvent("Click", ConfirmPush)
ConfirmPush(*) {
    global CommitMsgWinPath, CommitMsgWslPath
    global PushUrl, PushBranch, PushUpstream, PushAhead
    msg := CommitEdit.Value
    if (msg = "")
        msg := "Genesis updates from DESKTOP-2MI669I"

    RefreshRemote()

    if (PushUrl != "" && !InStr(PushUrl, "SWGfan")) {
        MsgBox("REFUSING TO PUSH.`n`n'origin' is:`n    " . PushUrl
             . "`n`nProject policy is that nothing is ever pushed anywhere but SWGfan."
             . "`n`nFix the remote first:`n"
             . "    git remote set-url origin https://github.com/SWGfan/SWGReturns.git",
             "Confirm Push", "Icon!")
        return
    }

    target := (PushUrl = "")
        ? "UNKNOWN -- no origin remote found."
        : PushUrl . "`n    branch " . PushBranch . "  →  " . PushUpstream
          . "`n    " . PushAhead . " commit(s) currently unpushed"

    result := MsgBox("PUSH TO:`n`n    " . target
                   . "`n`n----------------------------------------`n`n"
                   . "Commit message:`n`n" . msg
                   . "`n`nThis commits everything shown by Show Changes and pushes it.",
                   "Confirm Push", "YesNo Icon!")
    if (result != "Yes")
        return

    ; "UTF-8-RAW" not "UTF-8": the latter writes a byte-order mark, which ends up
    ; as an invisible character at the START of the commit message.
    f := FileOpen(CommitMsgWinPath, "w", "UTF-8-RAW")
    f.Write(msg)
    f.Close()

    BtnConfirmPush.Enabled := false
    SetLog("Committing and pushing to SWGfan...`n`nAllow a minute or two. GitHub credentials will be prompted for in a`nconsole window if they aren't cached -- watch the taskbar for it.")
    RunAsync("push --msgfile " . CommitMsgWslPath, PushDone)
}

PushDone(out) {
    SetLog(out)
    RefreshStatus()
    RefreshRemote()
}

RefreshConsole(*) {
    out := RunCapture("console_snapshot")
    out := Trim(out)
    ConsoleBox.Value := out = "" ? "(no output)" : out
}

; ---- startup + auto-refresh --------------------------------------------------
; Target is NOT polled: it only changes when you change it, or when their
; launcher runs. Refreshed at startup, after a switch, and on Refresh Status.
RefreshStatus()
RefreshConsole()
RefreshRemote()
RefreshTarget()
SetTimer(RefreshStatus, 5000)
SetTimer(RefreshConsole, 4000)

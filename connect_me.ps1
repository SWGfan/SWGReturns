# =====================================================================
#  Point the game client at the running server
#  Written by SWGReturn (Claude), 2026-08-15.
#
#  WSL2 gives the Linux side a new IP on most restarts, and after the
#  Windows reinstall the whole subnet moved (172.17.x -> 172.28.x).
#  The client cfgs still held the old address.
#
#  Two things have to match, and both are done here:
#    1. loginServerAddress0 in the client cfgs -> current WSL IP
#    2. the `galaxy` row's address -> the same IP.  The login server
#       hands this to the client after login, so if it stays 127.0.0.1
#       you connect, then fail at character select.
#
#  Same thing the control panel's "Play This Server" button does.
#  No server restart needed -- just restart the game client.
# =====================================================================

$ErrorActionPreference = 'Continue'

$Distro   = 'Ubuntu-24.04'
$Client   = 'C:\SWGEmu'
$GalaxyId = 3
$Cfgs     = @('swgemu.cfg', 'swgemu_login.cfg')

function Say($m, $c = 'Gray') { Write-Host $m -ForegroundColor $c }

Clear-Host
Say ''
Say '  ============================================================' Cyan
Say '   Point the client at the server' Cyan
Say '  ============================================================' Cyan
Say ''

# --- 1. current WSL IP ------------------------------------------------
$raw = & wsl.exe -d $Distro -u root -- hostname -I 2>&1
$ip  = ($raw -join ' ').Trim().Split(' ')[0].Trim()

if ($ip -notmatch '^\d{1,3}(\.\d{1,3}){3}$') {
    Say '  Could not read the WSL IP address.' Red
    Say ("  Got: {0}" -f $raw) Yellow
    Say '  Is the server distro running? Try starting the server first.' Yellow
    Say ''
    return
}
Say ("  Server (WSL) IP: {0}" -f $ip) Green

# --- 2. client cfgs ---------------------------------------------------
Say ''
Say '  Updating the client...'
foreach ($f in $Cfgs) {
    $p = Join-Path $Client $f
    if (-not (Test-Path $p)) { Say ("    {0}: not found" -f $f) Yellow; continue }

    $t   = Get-Content $p -Raw
    $old = ([regex]::Match($t, 'loginServerAddress0\s*=\s*([^\r\n]*)')).Groups[1].Value.Trim()

    if ($t -match 'loginServerAddress0\s*=') {
        $t = [regex]::Replace($t, '(loginServerAddress0\s*=\s*)[^\r\n]*', ('${1}' + $ip))
        [IO.File]::WriteAllText($p, $t)
        Say ("    {0}: {1}  ->  {2}" -f $f, $old, $ip) Green
    } else {
        Say ("    {0}: no loginServerAddress0 line" -f $f) Yellow
    }
}

# --- 3. the galaxy row ------------------------------------------------
Say ''
Say '  Updating the galaxy address in the database...'
$sql = "UPDATE galaxy SET address='$ip' WHERE galaxy_id=$GalaxyId; SELECT galaxy_id,name,address,port,pingport FROM galaxy;"
& wsl.exe -d $Distro -u root -- mysql -u root genesis -e $sql 2>&1 |
    ForEach-Object { Say ('    ' + $_) }

# --- done -------------------------------------------------------------
Say ''
Say '  ============================================================' Cyan
Say '   Done. Close the game if it is open, then start it again' Green
Say '   from C:\SWGEmu\SWGEmu.exe (not their launcher -- the' Green
Say '   launcher rewrites these settings).' Green
Say '  ============================================================' Cyan
Say ''
Say '  At the login screen, type any name and password you like --' Gray
Say '  the server creates the account automatically.' Gray
Say ''

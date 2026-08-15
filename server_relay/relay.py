#!/usr/bin/env python3
"""
SWG Genesis - UDP relay for WSL2 (Windows 10, NAT mode).

Listens on the Windows side for game traffic and relays it into the WSL VM
where core3 runs. Each outside player gets their own upstream socket so
replies route back to the right person.

WSL2 puts the server behind a second NAT layer, and Windows' own
`netsh interface portproxy` is TCP-only -- so UDP needs this relay.
Login, ping and zone are all UDP (verified in the source: LoginServer.idl
and ZoneServer.idl declare DatagramServiceThread, PingServer.h inherits it).

PORTS ARE READ FROM THE SERVER CONFIG, not hardcoded. The original version
of this file had 44453/44462/44463 baked in -- Companion's block. Genesis
runs on 46xxx, and a relay bound to the wrong ports forwards nothing and
reports no error: players just hang, because UDP login has no failure
message. Reading config-local.lua makes that class of bug impossible, and
lets anyone else host on whatever ports they configured.

Run:  python relay.py            (auto-detect everything)
      python relay.py 172.x.y.z  (explicit target IP override)

Env overrides: RELAY_PORTS, RELAY_TARGET, RELAY_BIND, RELAY_DISTRO, RELAY_CONF
"""

import asyncio
import os
import re
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)          # server_relay/ lives inside the repo root
CONF_DIR = os.path.join(REPO, "MMOCoreORB", "bin", "conf")

WSL_DISTRO = os.environ.get("RELAY_DISTRO", "Ubuntu-24.04")
BIND_ADDR = os.environ.get("RELAY_BIND", "0.0.0.0")
IDLE_TIMEOUT = 300          # seconds before a silent player session is dropped
WSL_IP_RECHECK = 60         # seconds between WSL IP re-checks
LOG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "relay.log")


def log(msg):
    line = "%s  %s" % (time.strftime("%Y-%m-%d %H:%M:%S"), msg)
    print(line, flush=True)
    try:
        with open(LOG_PATH, "a", encoding="utf-8") as f:
            f.write(line + "\n")
    except OSError:
        pass


# The three UDP services players actually need. StatusPort is TCP and only
# serves status queries, so it is deliberately not relayed. ORBPort is the
# internal object broker and must never be exposed.
PORT_KEYS = ["LoginPort", "PingPort", "ZoneServerPort"]


def read_ports():
    """Read the real ports out of the server's own config.

    config-local.lua wins over config.lua (it is parsed second by the server).
    Falls back to the genesis defaults only if nothing can be read at all.
    """
    env = os.environ.get("RELAY_PORTS")
    if env:
        return [int(p) for p in env.split(",")], "RELAY_PORTS override"

    found = {}
    source = None
    for name in ("config.lua", "config-local.lua"):   # local last, so it wins
        path = os.environ.get("RELAY_CONF") if name == "config-local.lua" else None
        path = path or os.path.join(CONF_DIR, name)
        if not os.path.isfile(path):
            continue
        try:
            text = open(path, encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        for key in PORT_KEYS:
            # matches "Core3.LoginPort = 46453" and bare "LoginPort = 46453",
            # ignoring commented-out lines
            m = re.findall(r"^[^-\n]*?\b(?:Core3\.)?%s\s*=\s*(\d+)" % key, text, re.M)
            if m:
                found[key] = int(m[-1])
                source = path

    ports = [found[k] for k in PORT_KEYS if k in found]
    if len(ports) == len(PORT_KEYS):
        return ports, source

    log("WARNING: could not read all ports from the config (%s). "
        "Falling back to genesis defaults -- CHECK THESE MATCH YOUR SERVER."
        % (", ".join(k for k in PORT_KEYS if k not in found) or "none missing"))
    return [46453, 46462, 46463], "built-in fallback"


def get_wsl_ip():
    """Ask WSL for its current internal IP. Starts WSL if it isn't running."""
    override = os.environ.get("RELAY_TARGET") or (sys.argv[1] if len(sys.argv) > 1 else None)
    if override:
        return override
    try:
        out = subprocess.check_output(
            ["wsl", "-d", WSL_DISTRO, "hostname", "-I"],
            text=True, stderr=subprocess.DEVNULL, timeout=60
        )
        parts = out.strip().split()
        if parts:
            return parts[0]
    except Exception as e:
        log("WSL IP lookup failed: %r" % (e,))
    return None


class UpstreamProtocol(asyncio.DatagramProtocol):
    """One per player session: the socket that talks to the WSL server."""

    def __init__(self, session):
        self.session = session

    def datagram_received(self, data, addr):
        self.session.touch()
        self.session.listener.transport.sendto(data, self.session.client_addr)

    def error_received(self, exc):
        pass  # ICMP unreachable etc. - session will idle out


class Session:
    def __init__(self, listener, client_addr):
        self.listener = listener
        self.client_addr = client_addr
        self.transport = None
        self.pending = []          # packets that arrived before upstream was ready
        self.last_seen = time.monotonic()
        self.target_ip = listener.wsl_ip

    def touch(self):
        self.last_seen = time.monotonic()

    def send(self, data):
        self.touch()
        if self.transport is not None:
            self.transport.sendto(data)
        else:
            if len(self.pending) < 256:
                self.pending.append(data)

    async def open(self):
        loop = asyncio.get_running_loop()
        try:
            self.transport, _ = await loop.create_datagram_endpoint(
                lambda: UpstreamProtocol(self),
                remote_addr=(self.target_ip, self.listener.port),
            )
        except OSError as e:
            log("port %d: cannot open upstream for %s: %r"
                % (self.listener.port, self.client_addr, e))
            self.listener.sessions.pop(self.client_addr, None)
            return
        for data in self.pending:
            self.transport.sendto(data)
        self.pending.clear()

    def close(self):
        if self.transport is not None:
            self.transport.close()
            self.transport = None


class ListenerProtocol(asyncio.DatagramProtocol):
    """The Windows-facing socket for one port."""

    def __init__(self, port, wsl_ip):
        self.port = port
        self.wsl_ip = wsl_ip
        self.sessions = {}
        self.transport = None

    def connection_made(self, transport):
        self.transport = transport

    def datagram_received(self, data, addr):
        sess = self.sessions.get(addr)
        if sess is None:
            sess = Session(self, addr)
            self.sessions[addr] = sess
            log("port %d: new session from %s:%d -> %s"
                % (self.port, addr[0], addr[1], self.wsl_ip))
            asyncio.get_running_loop().create_task(sess.open())
        sess.send(data)

    def reap_idle(self):
        now = time.monotonic()
        dead = [a for a, s in self.sessions.items() if now - s.last_seen > IDLE_TIMEOUT]
        for a in dead:
            self.sessions.pop(a).close()
            log("port %d: session %s:%d idle, closed" % (self.port, a[0], a[1]))

    def retarget(self, new_ip):
        """WSL restarted with a new IP: new sessions go there, old ones drain."""
        self.wsl_ip = new_ip


async def main():
    wsl_ip = None
    while wsl_ip is None:
        wsl_ip = get_wsl_ip()
        if wsl_ip is None:
            log("waiting for WSL to come up...")
            await asyncio.sleep(10)

    ports, src = read_ports()
    log("ports %s  (from %s)" % (ports, src))
    log("relaying UDP %s -> %s (%s)" % (ports, wsl_ip, WSL_DISTRO))
    loop = asyncio.get_running_loop()
    listeners = []
    for port in ports:
        try:
            _, proto = await loop.create_datagram_endpoint(
                lambda p=port: ListenerProtocol(p, wsl_ip),
                local_addr=(BIND_ADDR, port),
            )
        except OSError:
            log("ERROR: port %d is already in use - the relay is probably "
                "already running in another window. Only run ONE copy." % port)
            return
        listeners.append(proto)
        log("listening on %s:%d" % (BIND_ADDR, port))

    last_check = time.monotonic()
    while True:
        await asyncio.sleep(30)
        for l in listeners:
            l.reap_idle()
        if time.monotonic() - last_check >= WSL_IP_RECHECK:
            last_check = time.monotonic()
            current = get_wsl_ip()
            if current and current != listeners[0].wsl_ip:
                log("WSL IP changed %s -> %s; retargeting" % (listeners[0].wsl_ip, current))
                for l in listeners:
                    l.retarget(current)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        log("relay stopped")

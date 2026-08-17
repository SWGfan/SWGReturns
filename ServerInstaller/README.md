# SWG Genesis — Server Installer

Host your own Star Wars Galaxies server with the **Companion System**, on a
normal Windows PC, without knowing anything about code.

---

## What you need first

**1. A Windows PC** (Windows 10 or 11) with an internet connection.

**2. Your own copy of the Star Wars Galaxies client files.**
This installer does **not** include the base game. Those files belong to
LucasArts/SOE and we can't distribute them — same as every other SWG
emulator project. If you already play on any SWG emu server, you already
have them.

You'll be asked to point at that folder. It's the one containing
`bottom.tre`, `data_texture_00.tre` and so on.

Everything *custom* — the Companion System content and the Genesis world
content — is included and installed for you. You only need the base game.

**3. About an hour**, mostly unattended while it builds the server.

---

## Installing

1. Download the latest release.
2. Right-click **`INSTALL SERVER.cmd`** → **Run as administrator**.
3. Answer two questions: where your game files are, and where to put the server.
4. Leave it running.

It will restart your computer once. Run the same file again afterwards and
it picks up where it left off.

When it finishes, the control panel opens and tells you what to press.

---

## What it installs

Everything, so you don't have to:

- Windows Subsystem for Linux + Ubuntu
- The build toolchain and MariaDB
- Python (for the network relay) and AutoHotkey (for the control panel)
- The server source from [SWGfan/SWGReturns](https://github.com/SWGfan/SWGReturns), built from scratch
- The Companion System and Genesis content (downloaded for you)
- A UDP relay so players outside your network can reach you
- The control panel, on your desktop

Nothing is installed system-wide that you can't remove: the server lives in
its own Linux container, and uninstalling is one command.

---

## Playing

Players don't need any of this. They use the **Launcher**, which asks for
their game folder once and handles everything else — including keeping the
Companion System content up to date automatically.

---

## Opening your server to the internet

The installer sets up the relay and Windows firewall. You need to forward
three ports on your router to this PC:

| Port | Protocol |
|---|---|
| Login | **UDP** |
| Ping | **UDP** |
| Zone | **UDP** |

The installer prints your actual port numbers at the end — they depend on
your configuration, so use the ones it gives you rather than any you find
in a guide.

---

## Credits

Built on [SWGEmu](https://www.swgemu.com/) Core3, which is licensed under the
AGPL v3. The Companion System is by SWGfan.

## Licence

AGPL v3, matching Core3. See `LICENSE`.

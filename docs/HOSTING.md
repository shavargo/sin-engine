# Hosting `sin_ded` (Stock SiN Gold Steam Client Support)

Ship **engine only**. Do not redistribute Steam `*.sin` / `gamex86.dll`. The host
points `basedir` at a licensed SiN Gold / SiN 1 Multiplayer install (or a private tree copied from one).
Unmodified **stock SiN Gold clients from Steam** can connect directly to `sin_ded`.

## Why port 27910 (not 27015)

Stock Steam `sin.exe` **binds UDP 27015** for Steamworks even as a client.
Our dedicated server must use **27910** (Quake 2 default) so the binds do not
collide. Colliding on 27015 caused BEX (`c0000409`) right after `getchallenge`.

Stock’s *destination* default when you omit a port is still 27015
(`htons(0x6987)` in REA). Always connect with an explicit port:

```
connect 127.0.0.1:27910
```

Steam install also has alias `sin_local` in `base/autoexec.cfg` for that command.

## Bring-up (Windows)

```bat
sin_ded.exe ^
  +set basedir "C:/path/to/licensed/SiN 1 Multiplayer" ^
  +set dedicated 1 ^
  +set logfile 2 ^
  +set sv_steamauth 0 ^
  +set deathmatch 1 ^
  +set port 27910 ^
  +map biodm
```

Or use the build tree’s `sinroot` (already populated from a licensed copy):

```bat
cd path\to\sin-engine\build\Release
sin_ded.exe +set basedir <path-to-sin-engine>/build/Release/sinroot +set dedicated 1 +set logfile 2 +set sv_steamauth 0 +set deathmatch 1 +map biodm
```

Start **ded first**, then stock `sin.exe` (SiN Gold from Steam). In the client console:

```
connect 127.0.0.1:27910
```

`sv_steamauth 0` skips Steam ticket BeginAuthSession (REA only gates when the cvar is 1).

## Verified handshake

With ded on 27910 and `sv_steamauth 0`, OOB `getchallenge` → `challenge N` →
`connect 7 …` → `client_connect` is accepted (see connectionless `Com_Printf` lines
in `qconsole.log`).

## Master Server Registration & VPS Deployment

When running on a public VPS (Windows or Linux via Wine), `sin_ded` can advertise to classic UDP 27900 master servers (e.g. `master.shavargo.com:27900`) so players can discover and query the server.

### VPS Firewall Rules
* **UDP 27910 (Inbound)**: Game traffic port. Must be open to allow external clients to connect.
* **UDP 27900 (Outbound)**: Master server heartbeat port to announce to `master.shavargo.com`.

### Launching on a VPS
```bat
sin_ded.exe ^
  +set basedir "C:/path/to/SiN 1 Multiplayer" ^
  +set dedicated 1 ^
  +set sv_steamauth 0 ^
  +set deathmatch 1 ^
  +set port 27910 ^
  +set sv_master1 "master.shavargo.com:27900" ^
  +set hostname "My SiN VPS Server" ^
  +set logfile 2 ^
  +map biodm
```

### Linux VPS (Native ELF — Zero Wine)
`sin_ded` compiles directly as a native Linux binary.

1. **Build natively on Linux**:
   ```bash
   cmake -B build -DSIN_DEDICATED=ON -DSIN_MP=ON -DSIN_STATIC_GAME=ON
   cmake --build build --config Release
   ```
   *(Note: `-DSIN_STATIC_GAME=ON` compiles the game logic directly into `sin_ded` as a single, self-contained binary without needing any `.so` or `.dll` files).*

2. **Run natively**:
   ```bash
   ./build/sin_ded +set basedir /path/to/sin +set dedicated 1 +set sv_steamauth 0 +set deathmatch 1 +set port 27910 +set sv_master1 "master.shavargo.com:27900" +map biodm
   ```

### Linux VPS (Wine Fallback for Stock Windows DLL)
If using the proprietary Windows `gamex86.dll` directly from Steam:
```bash
wine sin_ded.exe +set basedir /path/to/sin +set dedicated 1 +set sv_steamauth 0 +set deathmatch 1 +set port 27910 +set sv_master1 master.shavargo.com:27900 +map biodm
```

### Console Commands
* `setmaster`: Views active master servers and public broadcast status.
* `setmaster <addr1> <addr2>...`: Configures master servers on-the-fly and sends immediate pings.
* `setmaster clear`: Clears all registered masters.
* `heartbeat`: Forces an immediate heartbeat broadcast to all active masters.

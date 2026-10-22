# REA → Port Deltas

Track Quake 2 / SiN MP changes against the companion reverse-engineering ledger in [`sin-mp-rea`](https://github.com/shavargo/sin-mp-rea) (or `../sin-mp-rea/docs/sin-exe-decomp/symbol-map.json` locally).

Status: `todo` | `stub` | `ported` | `verified`

| Addr | Symbol | Status | Notes |
|------|--------|--------|-------|
| — | `PROTOCOL_VERSION` | ported | SiN MP **7** via `SIN_MP` + `sin/sin_protocol.h` |
| `0x492510` | `SVC_GetChallenge` | stub | Slim-tokenize steamid; ticket ignored unless `sv_steamauth`; ded stays on **27910** (stock binds 27015) |
| `0x492710` | `SVC_DirectConnect` | stub | Protocol 7; Steam gate via `sv_steamauth`; static `client_t` temp; verified `client_connect` on 27910 |
| `0x4923f0` | `SVC_Info` | ported | Uses `PROTOCOL_VERSION` (7) |
| `0x493000` | `SV_ConnectionlessPacket` | ported | Stock dispatch; handlers patched |
| `0x481a90` | `Netchan_OutOfBandPrint` | ported | Stock Q2 |
| `0x49e0f0` | `Sys_GetGameAPI` | ported | Steam SiN API 14 via `Sin_FillGameImport` |
| — | `game_import_t` / `game_export_t` | ported | Ritual SDK layout; version 14; CreateSurfaces + consoles/surfaces |
| `0x454200` | `FS_LoadPackFile` (SPAK) | ported | 128-byte dir entries (`dsinpackfile_t`) |
| `0x478b40` | `CMod_LoadBrushModel` | ported | RBSP/1; 180-byte texinfo; **12-byte** SiN brushsides + lightinfo |
| — | `CreateSurfaces` | ported | Called pre-SpawnEntities with packed groupname surfaces |
| — | `IsModel` / `CalculateBounds` | ported | `.def`/`.sam` bounds (`sin/sinmdl.c`); labeled `idle1 foo.sam` |
| `0x4992e0` | `CSteamServer_BeginAuthSession` | stub | `sin/steam_server.c` (Steamworks TODO) |
| `0x4993b0` | `CSteamServer_GetAuthSessionTicket_Local` | stub | local listen path |
| `0x499260` | `CSteamServer_OnAuthTicketResponse` | stub | callback |
| `0x10013e60` | `GetRefAPI` (ref_gl) | partial | RBSP/texinfo; TGA colormap/conback; **GL_LoadSWL**; SINMDL draw TODO |
| `0x10008360` | `GL_LoadSWL` | ported | name@0 w/h@0x40 palette@0x48 RGBA offsets@0x44c; magenta→A=0 |
| — | `SINMDL_*` (engine + ref_gl) | partial | Server cache verified (biodm fell-out **0**); ref draw TODO |
| — | `CL_WriteConfiguration` | ported | `MAX_OSPATH` for config.cfg path |
| — | `FS_ExecAutoexec` | ported | `MAX_OSPATH` (was overflowing on long basedirs) |
| — | `InitConProc` | ported | NULL-init qhost handles (uninit garbage → false qhost) |

## Working Method

1. Select an unported/stub entry from the table above or the REA symbol map.
2. Review the decompiled pseudocode in `sin-mp-rea`.
3. Implement clean, compliant C code (preferring `sin/` modular hooks to avoid altering stock Q2 engine paths unnecessarily).
4. Verify functionality and update the status in this ledger.

# Phase 3 — Drop-in Steam layout

## Stock Steam install

`C:\Program Files (x86)\Steam\steamapps\common\SiN 1 Multiplayer\`

| File | Role |
|------|------|
| `sin.exe` | Engine (replace with `sin_client` / dedicated) |
| `ref_gl.dll` | OpenGL renderer |
| `ref_soft.dll` | Software renderer (optional) |
| `steam_api.dll` | Valve |
| `base\gamex86.dll` | Game |
| `base\*.sin` | Assets |
| `ds_deathmatch.bat` / `ds_sinctf.bat` | Dedicated launchers |

## Drop-in procedure (when binaries are ready)

1. Backup `sin.exe`, `ref_gl.dll`
2. Copy build outputs next to Steam install (or side-by-side `sin-mp\` folder with `+set basedir` to Steam `base`)
3. Prefer **side-by-side** first — do not overwrite Steam files until verified
4. Match cvars from stock dedicated bats (`+set dedicated 1`, deathmatch/ctf)

## Stock client → our `sin_ded`

See [HOSTING.md](HOSTING.md). Short version: run `sin_ded` on **27910** with
`sv_steamauth 0` and a licensed `basedir`; in stock console run
`connect 127.0.0.1:27910` (or `sin_local`). Do not put the ded on 27015 while
Steam `sin.exe` is running.

## Gap checklist vs REA

See `DELTAS.md`. Phase 3 closes only **behavior gaps** that block host/join parity — not all 1296 FUN_*.

# sin-engine

[![License: GPL v2](https://img.shields.io/badge/License-GPLv2-blue.svg)](LICENSE)
[![Platform: Win32](https://img.shields.io/badge/Platform-Win32-lightgrey.svg)](CMakeLists.txt)
[![Protocol: Version 7](https://img.shields.io/badge/Protocol-Version%207-orange.svg)](sin/sin_protocol.h)
[![CI Build](https://github.com/shavargo/sin-engine/actions/workflows/ci.yml/badge.svg)](.github/workflows/ci.yml)

An open-source engine chassis for **SiN 1 Multiplayer**, built on the id Software **Quake II** GPL codebase with protocol, asset, and renderer compatibility for the Steam release.

> [!TIP]
> **Dedicated Server**: `sin_ded.exe` is functional and works out-of-the-box with the unmodified **stock SiN Gold client from Steam** as well as `sin_client.exe`!
>
> **Client Executable Status**: `sin_client.exe` features modernized SDL2 platform support, widescreen resolutions, authentic 3D menus, model rendering, viewmodel attachments, and client-side weapon effects. Advanced particle and beam systems are in active development.

---

## Notice & Asset Disclaimer

> [!IMPORTANT]
> **This repository distributes source code only.**
> No copyrighted game assets (maps, textures, sounds, models, or `.sin` archive files) or proprietary binaries are included. To run or test this engine with game content, you must own a legitimate copy of **SiN 1 Multiplayer / SiN Gold** (available on Steam).
>
> *SiN* is a trademark of Ritual Entertainment / Nightdive Studios / Activision. *Quake II* is a trademark of id Software / ZeniMax Media. This project is an independent open-source recreation and is not affiliated with or endorsed by Ritual Entertainment, id Software, Valve Corporation, or Nightdive Studios.

---

## Project Goals

1. **Dedicated Server (`sin_ded.exe`)**: A stable, headless dedicated server capable of hosting games for unmodified **stock SiN Gold clients from Steam**.
2. **Client & Renderer (`sin_client.exe`, `ref_gl.dll`)**: A modernized 32-bit client and OpenGL renderer supporting SiN's extended file formats (SPAK, SWL, RBSP, SINMDL).
3. **Drop-in Engine Replacement**: A drop-in binary replacement for the original retail/Steam executable.

---

## Current Status

### Dedicated Server (`sin_ded.exe`)
| Subsystem | Status | Highlights |
|-----------|--------|------------|
| **Server Host** | :white_check_mark: Functional | Works out-of-the-box with the unmodified **stock SiN Gold Steam client** as well as `sin_client.exe`. Protocol 7, `SVC_GetChallenge`, `SVC_DirectConnect`, `sv_steamauth 0` |
| **Game DLL Interface** | :white_check_mark: Functional | Steam SiN `gamex86.dll` (API 14) and native `game_sin` submodule load; `InitGame` & `SpawnEntities` work |
| **Asset Loading** | :white_check_mark: Functional | 128-byte SPAK headers, RBSP/1 BSPs, 12-byte SiN brushsides, `.swl` textures, and `.sin` archives |
| **Collision / Bounds** | :white_check_mark: Functional | Model bounds via `.def`/`.sam`, zero fell-out errors on `biodm`, authentic `trace_t` + `MASK_PROJECTILE` |
| **Projectile Physics** | :white_check_mark: Functional | Rocket & Grenade Launcher bounce, deflection angles, damage multipliers, and correct world coordinate propagation |
| **Client Join** | :white_check_mark: Functional | Clients connect, parse challenge and connect packets, download/load baselines, and enter the game |

### Client & Renderer (`sin_client.exe`, `ref_gl.dll`)
| Subsystem | Status | Highlights |
|-----------|--------|------------|
| **OpenGL Renderer** | :white_check_mark: Functional | SWL texture loading with palette decode, conback `.tga`, SINMDL 3D mesh rendering, depth-hack for first-person models, translucent glass/window alpha sorting, SiN `.spr` text sprite effects, and multi-skin support |
| **Platform Layer** | :white_check_mark: Functional | Modern SDL2 backend with widescreen/high-DPI display, raw relative mouse input, borderless fullscreen, and OGG audio streaming |
| **3D Menu & Console** | :white_check_mark: Functional | Authentic SiN 3D interactive main menu (`menu3d.c`) and 3D textured console (`console3d.c`) |
| **View Weapon Models** | :construction: In Progress | Parented first-person weapon models attach properly to player; idle, firing, and reload animation state transitions and loop behaviors are under active refinement |
| **Client Effects System**| :construction: In Progress | Initial `.def` frame command execution framework (`cl_modelcmds.c`) implemented; texture binding for spawned effect models/sprites, particle textures, and specialized beams (`shootbeam`) are being addressed |
| **Client Playability** | :construction: In Progress | In-game rendering, movement, and multiplayer connectivity are functional; advanced particle systems, weapon beams, and world surface translucency (glass) are ongoing |

### Known Limitations & Immediate Worklist

The following items are actively identified for resolution when development resumes:

- **Weapon View Model Animations**: View model animation playback issues (weapons getting stuck in loops, failing to cycle, or jittering during state transitions).
- **Specialized Weapon Effects**: Beam weapons (`shootbeam` for Bee, Stunner, Lightning), `explosion` events, and footstep sound dispatch (`footstep`).
- **Steamworks Auth**: Client auth validation is currently stubbed (`sv_steamauth 0` bypasses ticket check).

---

## Directory Structure

```
sin-engine/
├── .github/          # GitHub Actions CI, issue & PR templates
├── client/           # Quake 2 client engine
├── server/           # Quake 2 server engine
├── qcommon/          # Common engine utilities and network channel
├── win32/            # Win32 platform layer, input, and audio
├── ref_gl/           # OpenGL hardware renderer (with SiN SWL, RBSP, and SINMDL support)
├── ref_soft/         # Software renderer (legacy)
├── game/             # Stock Quake 2 game DLL (used for clean baseline bring-up)
├── sin/              # SiN Multiplayer deltas (protocol 7, SPAK, SINMDL, Steam stubs)
└── CMakeLists.txt    # Modern CMake configuration
```

---

## Building with CMake

### Prerequisites

- **Windows 10 / 11**
- **Visual Studio 2022 or 2026** (with C++ Desktop Development workload)
- **CMake 3.16+** (included with Visual Studio or standalone)

### Client & Renderer Build

> [!NOTE]
> While `sin_client.exe` is functional, it is not playable.

To configure and build the client executable (`sin_client.exe`), OpenGL renderer (`ref_gl.dll`), and baseline game DLL (`gamex86.dll`):

```cmd
cmake -B build -A Win32 -DSIN_DEDICATED=OFF -DSIN_MP=ON -DSIN_BUILD_CLIENT=ON -DSIN_BUILD_REF_GL=ON -DSIN_BUILD_GAME=ON
cmake --build build --config Release
```

Build outputs:
- `build/Release/sin_client.exe`
- `build/Release/ref_gl.dll`
- `build/baseq2/Release/gamex86.dll`

### Dedicated Server Build

To configure and build only the lightweight dedicated server (`sin_ded.exe`):

```cmd
cmake -B build-ded -A Win32 -DSIN_DEDICATED=ON -DSIN_MP=ON -DSIN_BUILD_GAME=ON
cmake --build build-ded --config Release
```

Build outputs:
- `build-ded/Release/sin_ded.exe`
- `build-ded/baseq2/Release/gamex86.dll`

---

## Running a Dedicated Server (Connecting with Stock SiN Gold Client)

`sin_ded.exe` works directly with the unmodified **stock SiN Gold client from Steam**.

Stock Steam SiN clients bind UDP port **27015** for internal Steamworks hooks. To prevent port collisions when running client and server on the same system, `sin_ded` defaults to port **27910** (the Quake 2 standard port).

```cmd
sin_ded.exe +set basedir "C:/path/to/Steam/steamapps/common/SiN 1 Multiplayer" +set dedicated 1 +set sv_steamauth 0 +set deathmatch 1 +set port 27910 +map biodm
```

From the stock Steam SiN Gold client console:
```
connect 127.0.0.1:27910
```

---

## Contributing

Contributions, bug reports, and pull requests are welcome! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for details on code style and pull request procedures.

---

## License

This project is licensed under the **GNU General Public License Version 2** — see the [LICENSE](LICENSE) file for the full text.

Original Quake II source code:
> Copyright (C) 1997-2001 Id Software, Inc.

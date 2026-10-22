# Phase 1 — Game DLL

## What works now

`sin_ded` loads stock Quake 2 `gamex86.dll` built from `game/` (smoke: engine init + `Sys_GetGameAPI`).

```
build/baseq2/Release/gamex86.dll  →  copy to  build/Release/baseq2/gamex86.dll
```

## Steam SiN `base\gamex86.dll`

Steam path: `<Steam install>\SiN 1 Multiplayer\base\gamex86.dll`

Engine side: `sin/sin_game.h` + `sin/sin_sv_game.c` (`Sin_FillGameImport`). Verified: InitGame (306 classes), SpawnEntities on `biodm`.

Known gaps: SINMDL import stubs; full SiN delta entity protocol.

## Building and Verification

Build dedicated server and game DLL using CMake:

```cmd
cmake -B build -A Win32 -DSIN_DEDICATED=ON -DSIN_MP=ON -DSIN_BUILD_GAME=ON
cmake --build build --config Release
```

Run dedicated server with stock game DLL:

```cmd
cd build\Release
sin_ded.exe +set dedicated 1 +set developer 1 +set sv_steamauth 0 +set deathmatch 1 +map q2dm1
```
*(Expect: Winsock Initialized / SinSteam stub / Quake2 Initialized)*

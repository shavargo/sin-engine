# Implementation status (plan phases)

| Phase | Status | Evidence |
|-------|--------|----------|
| Bootstrap `sin-engine` | done | Tree + README + DELTAS + CMake |
| Phase 1 dedicated server | verified | `sin_ded.exe` builds and hosts games for the **stock SiN Gold client from Steam** |
| Phase 1 protocol/Steam | done | `PROTOCOL_VERSION` 7; challenge/connect Steam stubs |
| Phase 1 game DLL | done | Steam SiN `gamex86` API **14**; InitGame + SpawnEntities on `biodm` |
| Phase 1 Steam assets | done | SPAK 128-byte; RBSP/1; 12-byte SiN brushsides; CreateSurfaces; CS table |
| Phase 1 collision | done | biodm `fell out` **0** after SINMDL `.def`/`.sam` bounds; authentic `trace_t` + `MASK_PROJECTILE` |
| Phase 1 projectiles | done | Rocket and Grenade Launcher physics, deflection, and bounce lifecycles verified |
| Phase 2 client net | partial | Connect works (`Blade connected` + `Map: biodm`); extras stub |
| Phase 2 ref_gl / SINMDL | done | RBSP + SiN texinfo; TGA palette/RLE; `.swl` load (per-tex palette); conback `.tga`; SINMDL 3D mesh + multi-skin draw |
| Phase 2 client join | verified | biodm connect; walls via SWL; path overflows fixed (`MAX_OSPATH`) |
| Phase 3 drop-in | docs | `docs/DROPIN.md` + hosting guides |

## Known Limitations & Next Steps

- **Steamworks**: Auth ticket verification is stubbed out (`sv_steamauth 0`).
- Next: Steamworks auth integration and client net extras.

# Contributing to sin-engine

Thank you for your interest in contributing! This project is an open-source engine reconstruction for **SiN 1 Multiplayer**, built on the GPL Quake 2 engine codebase.

## Legal and Ethical Guidelines

- **No Proprietary Game Assets:** Do not commit or submit proprietary game content (`.sin` packfiles, maps, textures, sounds, original game DLL binaries, etc.). Users must supply their own assets from a legally purchased copy of SiN 1 Multiplayer (e.g. from Steam).
- **Clean Implementation:** Contributions should be clean C code adhering to C99 and MSVC compatibility. Do not paste raw Ghidra or disassembler output into the repository.
- **License Compliance:** All contributions will be licensed under the **GNU General Public License v2 (GPL-2.0)**.

## Development Setup

1. **Prerequisites:**
   - Visual Studio 2022 or 2026 (with Desktop development with C++)
   - CMake 3.16+ (or the CMake bundled with Visual Studio)
   - Git

2. **Building with CMake:**
   ```cmd
   # Configure client, OpenGL renderer, and game DLL
   cmake -B build -A Win32 -DSIN_DEDICATED=OFF -DSIN_MP=ON -DSIN_BUILD_CLIENT=ON -DSIN_BUILD_REF_GL=ON -DSIN_BUILD_GAME=ON

   # Build Release
   cmake --build build --config Release
   ```

   For a dedicated-only server build:
   ```cmd
   cmake -B build-ded -A Win32 -DSIN_DEDICATED=ON -DSIN_MP=ON -DSIN_BUILD_GAME=ON
   cmake --build build-ded --config Release
   ```

## Pull Request Guidelines

- Ensure your changes build cleanly under Win32 Release configurations without breaking existing targets (`sin_ded`, `sin_client`, `ref_gl`, `gamex86`).
- Prefer placing SiN-specific additions into `sin/` and hook into engine subsystems cleanly.
- Document any protocol or packet format changes in `DELTAS.md` or the relevant documentation files.
- Keep commits atomic and write clear commit messages.

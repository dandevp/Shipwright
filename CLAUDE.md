# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

Ship of Harkinian (SoH) is a PC port of Ocarina of Time built from the OoT decompilation, running on **libultraship** (LUS, a modern reimplementation of the N64 libultra SDK: rendering, audio, input, resource loading, ImGui, CVars). No copyrighted assets ship with the repo: they are extracted from a user-supplied ROM into `.o2r` archives at build/run time. Main branch for PRs is `develop`.

Submodules `libultraship/` and `torch/` must be initialized: `git submodule update --init`.

## Build (Linux/macOS)

```bash
cmake -H. -Bbuild-cmake -GNinja                  # add -DCMAKE_BUILD_TYPE:STRING=Release for packaging
cmake --build build-cmake --target GenerateSohOtr # builds soh.o2r (SoH's own custom assets)
cmake --build build-cmake                         # binary: build-cmake/soh/soh.elf (macOS: soh-macos)
```

- `-DSUPPRESS_WARNINGS=0` shows warnings from LUS and decomp `soh/src` (suppressed by default).
- `-DUSE_ASAN=ON` enables AddressSanitizer.
- `ExtractAssets` target extracts `oot.o2r`/`oot-mq.o2r` from ROMs in `roms/` (or `-DSOH_ROM_PATH=...`) using the `soh-torch` tool. The game can also extract at first launch.
- `ExtractAssetHeaders` is currently non-functional (Torch has no OoT header exporter); the headers checked into `soh/assets` must be edited by hand.
- Dependency lists per distro: `linux-build-deps/*.txt`; Nix: `nix develop ./linux-build-deps`. Windows/Switch/Wii U: see `docs/BUILDING.md`.

There is no automated unit-test suite; verification is building and running the game.

## Formatting / lint (all enforced by CI)

- **clang-format 14 exactly** (15+ produces different output and fails CI): `./run-clang-format.sh` (override binary with `CLANG_FORMAT=...`, e.g. `CLANG_FORMAT="uvx clang-format@14"`). It skips `soh/src`, `soh/include`, `soh/assets`.
- Whitespace (spaces, no trailing whitespace): `./scripts/check-whitespace.sh [--fix]`
- JSON in `soh/assets/custom`: `./scripts/lint-json.sh [--fix]`
- Optional pre-commit hook runs all three: `pre-commit install --install-hooks`

## Architecture

- `soh/src/` — the decompiled OoT C code (`code/`, `overlays/actors|gamestates|effects|...`, `libultra/`). Treat as vanilla game logic; keep edits minimal. Instead of rewriting behavior here, insert hook points (see below). `soh/include/` holds decomp headers (`z64.h`, `macros.h`, `variables.h`, `functions.h`).
- `soh/soh/` — SoH's C++ port layer and features:
  - `OTRGlobals.cpp` — startup/glue between LUS and the game; C-callable bridge functions.
  - `Enhancements/` — nearly all features (QoL, cheats, cosmetics, randomizer, boss rush, trackers, savestates, etc.).
  - `Enhancements/randomizer/` — randomizer; `3drando/` contains the seed-generation logic ported from OoT3D Randomizer, `location_access/` the per-region logic.
  - `SohGui/` — ImGui menus. Menu entries are declared with `AddWidget(path, "Label", WIDGET_CVAR_...).CVar(...)` in `SohMenu*.cpp`.
  - `resource/` — LUS resource types/factories for OoT-specific asset formats loaded from `.o2r`.
  - `Network/` — remote integrations (Crowd Control, Sail, Anchor).
- `soh/assets/` — checked-in asset headers, `yml/` extraction specs for Torch, `custom/` SoH-owned assets packed into `soh.o2r`.
- `torch/` — asset extractor (from N64 ROMs); `libultraship/` — platform layer.

### Feature pattern: CVars + GameInteractor hooks + ShipInit

Modern features avoid scattering `if (CVarGet...)` through decomp code. Instead:

1. **CVars** store settings. Always build names with prefix macros from `soh/soh/cvar_prefixes.h` (`CVAR_ENHANCEMENT("Foo")`, `CVAR_CHEAT(...)`, `CVAR_COSMETIC(...)`, `CVAR_RANDOMIZER_SETTING(...)`, ...). Read with `CVarGetInteger/CVarGetFloat(name, default)`.
2. **GameInteractor** (`Enhancements/game-interactor/`) is the hook bus. Decomp code calls `GameInteractor_ExecuteOn<Event>()` at event points; hook types are listed in `GameInteractor_HookTable.h`.
   - **Vanilla-behavior hooks**: decomp code is wrapped as `if (GameInteractor_Should(VB_SOMETHING, <vanilla condition>, args...))`. Features flip `*should` via `COND_VB_SHOULD(VB_SOMETHING, condition, { ... })`. Each `VB_` id is documented in `vanilla-behavior/GIVanillaBehavior.h` with its vanilla result and its `va_arg` args — add new ids there with the same doc format.
   - `COND_HOOK(OnFoo, condition, lambda)` / `COND_ID_HOOK(...)` register or unregister a hook depending on `condition`.
3. **ShipInit** (`soh/soh/ShipInit.hpp`): a feature file defines a `RegisterX()` function containing its `COND_*` registrations and registers it with `static RegisterShipInitFunc initFunc(RegisterX, { CVAR_NAME, ... });`. It runs at boot and again whenever a listed CVar is changed through a UI widget (`UIWidgets.cpp` calls `ShipInit::Init(cvarName)`), so hooks re-register when settings change. Special paths like `"IS_RANDO"` are also used.

See `soh/soh/Enhancements/SpeedModifiers.cpp` for a representative example, and `docs/MODDING.md` for a walkthrough of adding a CVar-backed option.

### C/C++ boundary

Decomp code is C; SoH code is mostly C++. C++ files include decomp headers inside `extern "C" { ... }` and declare needed decomp globals/functions there (e.g. `extern PlayState* gPlayState;`). Functions callable from C are exposed via `extern "C"` wrappers (e.g. in `OTRGlobals.h`, `GameInteractor.h`).

### Agent Notes
Always update the features.md when updating the code

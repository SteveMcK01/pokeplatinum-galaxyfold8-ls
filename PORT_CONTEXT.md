# Pokémon Platinum Android Port (`pokeplatinum-galaxyfold8-ls`) — Technical Context

## 1. Project Identity & Workflow Rules
- **Repository & Active Branch:** `SteveMcK01/pokeplatinum-galaxyfold8-ls` (`platinum-thor` branch).
- **Base ROM & Mod Setup:** Vanilla *Pokémon Platinum* (USA) decompilation (`pokeplatinum`) with a C rewrite of the **Following Pokémon** mod (`src/port/following_hack.c`) and an embedded **Gen 5 (`pokebw2`)** battle/summary engine (requires a user-supplied *Black 2* / *White 2* USA ROM at runtime). Note: This is **not** Renegade Platinum.
- **Target Platform (Android-Only):** Exclusively targets **Android `arm64-v8a`** (`org.pokeplatinum.port`), optimized for foldables (Samsung Galaxy Z Fold) and phones. **Never** run or suggest Linux/Windows desktop builds.
- **Build & Deployment Pipeline:**
  - **Build System:** Meson + Ninja + Android NDK + Gradle (`android/`, `tools/android/build_apk.sh`).
  - **CI/CD:** Pushing to `platinum-thor` triggers `.github/workflows/build_android.yml`, which builds a signed APK with `VERSION_CODE=$((9100000 + RUN_NUMBER))`.
  - **Shell Safety:** Always wrap multi-step terminal commands in a subshell `( ... )` and avoid top-level `set -e` so failures do not close the terminal. Every code-change command should stage, commit, and push (`git add ... && git commit -m "..." && git push`) and keep `AGENTS.md` updated.
  - **Gen 5 Patch Workflow:** Keep `pokebw2` outside the repo (`/tmp/pokebw2` at commit `2c3961614d0fe1971ad0535e6edbbf3a6078e7b2`) and generate patches strictly via `gen5/tools/amend_patch.py` into `gen5/patches/`.
- **Antigravity Model & Mode Advisory Rule:** At the start of every new bug or feature—and immediately if a pushed CI build fails to resolve an issue—explicitly recommend the optimal Antigravity configuration before proposing edits:
  - **Routine C/C++ edits, UI scaling, QoL wiring, Meson/CI scripts, or doc updates:** Recommend **Orchestrator · Gemini 4 Neon (Medium)** in **Build** mode.
  - **Multi-file subsystem bugs, heap/SysTask lifecycle crashes, ARM64 ABI issues, or `pokebw2` patches:** Recommend switching to **Plan** mode (`/plan`) with **Orchestrator · Gemini 4 Neon (High)** first to verify the root cause before switching to **Build** mode to commit and push (since Android builds run remotely on GitHub Actions rather than locally).
  - **Complex 3D/2D `libntr` hardware register emulation, GLSL shader compositing (`g2_fragment.glsl`), or bugs where a previous patch did not resolve the symptom (including the active Route 210 North fog white-screen bug):** Recommend **Plan** mode with **Orchestrator · Gemini 4 Neon (High)**, and advise escalating to **Orchestrator · Gemini 4 Carbon (High)** (or `/boost`) for the root-cause turn if the register/shader interaction is non-obvious, then dropping back to **Neon (Medium/High)** in **Build** mode to apply and push.
- **Deep-Dive Documentation Index:** Always consult `AGENTS.md` and [`docs/architecture/README.md`](docs/architecture/README.md) (`docs/architecture/core/*` and `docs/architecture/features/*`) before modifying core subsystems.

---

## 2. Port Architecture, Libraries & APIs

### Android / JNI / SDL2 Entry & Threading
- **Java/JNI Host (`android/app/`, `libSDL2.so`):** `org.libsdl.app.SDLActivity` / `SDLMain` initializes audio, touch/controller input, and the GL surface, then invokes `Java_org_libsdl_app_SDLActivity_nativeRunMain`.
- **Main Game Loop (`libmain.so` on `SDLThread`):**
  `SDL_main` -> `NitroMain`, which executes each frame in two phases:
  1. `ApplicationManager_Exec`: Runs the active overlay/application state machine (`FieldMap`, `LibraryTV`, menus, or battles).
  2. `SysTaskManager_ExecuteTasks`: Ticks all registered per-frame `SysTask` callbacks (map object movement/animations, overworld anim managers, weather, UI prompts).
- **Thread-Safety Rule:** Game state (`FieldSystem`, `SaveData`, `Party`, `Bag`, `Heap_*`) belongs to `SDLThread`. Cross-thread UI/companion reads/writes must follow the synchronization rules in `docs/architecture/core/save-party-bag-and-threading.md`.

### Graphics, UI & Networking Stack
- **Graphics API — OpenGL ES 3.x + GLSL (NOT Vulkan):**
  - **`subprojects/libntr`:** Custom Nintendo DS (Nitro SDK) hardware simulation library compiled as a Meson subproject. Translates DS GX/G2 (2D) and G3 (3D) hardware commands/registers (`libraries/gx/src/g2.c`, `libraries/sim/src/g3_draw.cpp`) into OpenGL ES draw calls and GLSL shaders (`libraries/sim/src/shaders/g2_fragment.glsl` composites `BG0`–`BG3`, `bg0as3d` 3D output, `BLDCNT`/`BLDALPHA`/`BLDY` color special effects, and master brightness for Engine A and Engine B).
  - **Foldable Display Layout:** Renders Engine A (top screen, widescreen 3D at 60 FPS) full-screen with a floating, scalable Picture-in-Picture (PiP) rounded cyan bezel for Engine B (bottom DS touch screen), plus split-screen mode with the **Gear** companion panel.
- **Overlay & Companion UI — Dear ImGui:**
  - Embedded in `subprojects/libntr/include/simulator/imgui/imgui.hpp` and driven by `src/port/touch_controls.cpp`, `src/port/gear_data.c`, and `port_common_large_ui.patch`.
  - Powers the Settings/Cheats menus, Gear companion tabs (Party, Portable PC, Route Encounter/Dex Radar), and the full-screen Gen 5 Touch Battle HUD (2x2 action/move/target cards, effectiveness pills, `YES`/`NO` prompts, and quick-throw Poké Ball carousel).
- **Embedded Gen 5 Engine (`gen5/`, `src/port/gen5_*.{c,cpp}`):**
  - Compiles `pokebw2` directly into `libmain.so` with `bw2_*` symbol prefixing to run wild, roamer, Honey Tree, legendary/static, and trainer battles (plus Pokémon Summary and move-forget screens) using BW2 cries/SFX over Platinum BGM.
- **Networking:** **ENet** for local wireless/link play.

### Memory, Heaps & Static Overlay Lifecycle on ARM64
- **Compile-Time Macros:**
  - `SDK_PORT`: Enables cross-platform C/C++ port paths (64-bit pointers, `libntr` simulator hooks, static overlay linking).
  - `SDK_BUILD_ARM`: Defined only on original 32-bit DS ARM builds; **not** defined on Android `arm64-v8a` (critical whenever pointer sizes, script word offsets, or hardware register layouts differ).
- **Static Overlays:** All DS overlays (`overlay005` fieldmap, `overlay006`, `overlay008` gym features, `overlay009` Distortion World, etc.) are statically linked into `libmain.so`.
- **Sub-Application Heap Teardown Hazard (`HEAP_ID_FIELD1`):**
  - When `FieldSystem` launches a sub-application (e.g., `StartLibraryTV`, Bag, Party, Summary) and returns via `ReturnToField`, `FieldSystem` (`playerAvatar`, `mapObjMan`, `location`, and global `SysTaskManager`) stays alive, **but `FieldMap_Exit` (`src/overlay005/fieldmap.c`) destroys `HEAP_ID_FIELD1`**.
  - Any `SysTask` whose parameter struct was allocated on `HEAP_ID_FIELD1` **must** call `SysTask_Done` and free its data *before* `Heap_Destroy(HEAP_ID_FIELD1)`, or `SysTaskManager_ExecuteTasks` will read reused/corrupted heap memory during the sub-application transition.

---

## 3. Solved Bugs & Crashes (Do Not Re-Investigate)

### A. Canalave Library 3F TV Cutscene Crash (`MapObject_CheckStatusFlag+0` `SIGSEGV`)
- **Symptom:** Consistent `SIGSEGV` (`fault addr 0x0`) during the `StartLibraryTV` -> `ReturnToField` cutscene in `res/field/scripts/scripts_canalave_library_3f.s:146–152`.
- **Traps Ruled Out:**
  - An older Nov 20 `MapObject_GetX+0` (`x1 = 0x2e`) crash in the same log file was actually Pastoria City, not Canalave Library.
  - Changing `ScrCmd_ReturnToField` to `return FALSE` broke map reloading (`FieldSystem_GetVarPointer+100` crash); `return TRUE` is required so `ScriptContext` yields until `FieldMap_Init` finishes.
- **Root Cause:** ARM64 backtrace/register decoding mapped `#02` -> `MapObject_DoAnimation` (`src/unk_020655F4.c`), `#01` -> `MovementAnimation_CheckSet`, `#00` -> `MapObject_CheckStatusFlag(NULL, MAP_OBJ_STATUS_0)`. `ScrCmd_FollowerAnimation` (`src/port/following_hack.c:1716`) called `MapObject_StartAnimation(follower, cmds)` and discarded the `SysTask *` handle without ever calling `MapObject_FinishAnimation`. That leaked a permanent `MapObject_DoAnimation` `SysTask` whose `MoveAnimData` sat at `HEAP_ID_FIELD1 + 0x187F90` (`Heap_AllocAtEnd`). When `FieldMap_Exit` destroyed `HEAP_ID_FIELD1`, `LibraryTV` reused that memory, overwriting `animData->state` to `1` (`MOVEMENT_ANIM_STATE_CHECK_SET`) and `animData->mapObj` to `NULL`.
- **Fix Shipped:**
  1. Added `MapObject_SetAnimationAutoFinish(task, TRUE)` in `src/unk_020655F4.c` and enabled it in `ScrCmd_FollowerAnimation` so follower hop tasks automatically call `MapObject_FinishAnimation` on `MOVEMENT_ACTION_END`.
  2. Added `MapObject_CancelAllAnimations()` in `FieldMap_Exit` (`src/overlay005/fieldmap.c`) immediately before `Heap_Destroy(HEAP_ID_FIELD1)`.
  3. Added `PlayerAvatar_EnsureMapObject` (`src/player_avatar.c`) plus `NULL` guards across `MapObject_CheckStatusFlag`, `MapObject_CheckStatus`, `MapObject_DoAnimation`, `ScrCmd_RestoreCamera`, `ScrCmd_GetPlayerMapPos`, `FieldMapChange_DeleteObjects`, and `HandleFieldInput`.

### B. Pastoria City Cutscene & Script `MapObject` Null Crashes (`MapObject_GetX+0`, `e4a95264a`)
- **Root Cause:** Unspawned NPC local IDs (`0x2e`) and follower IDs (`0xF4`/`0xFD`) returned `NULL` in `ScrCmd_Unused_06A` (`GetObjectPos`) and 12 other `MapObject` script commands, dereferencing `NULL + 0x64` in `MapObject_GetX`.
- **Fix Shipped:** Routed lookups through `GetLocalMapObjByIndex` and added `NULL` early-returns across `src/scrcmd.c`.

### C. Gen 5 Battle Engine Routing, Type Offsets, Yes/No HUD & EXP Share (`ee98ff59d`, `0d6b198b3`)
- **Fixes Shipped:**
  - Routed static/legendary wild encounters to Gen 5 and fixed Platinum fallback battle input/quick-throw (`ee98ff59d`).
  - Offset BW2 `PokeType` IDs `>= 9` (`Fire`..`Dark`) by `+1` in `Gen5_GetBattleState` (`gen5/src/runtime.c`) to match Platinum's `TYPE_MYSTERY = 9`.
  - Checked `BTLV_INPUT_SCREEN_YES_NO` before `scd->numProcs == 0` in `Gen5_PortQueryBattleMenu` (`gen5/patches/src/ov169/btlv_scd.c.patch`).
  - Wired Gen 6-style party-wide Modern EXP Share into Gen 5 battles (`0d6b198b3`).

### D. Foldable Companion UI Scaling & CI Stability (`3c70423e5`..`c8dac3d72`)
- **Fixes Shipped:** Enlarged Gear companion screens and Party move-pill cards via clean incremental `port_common_large_ui.patch`; fixed Ubuntu 24.04 `apt` IPv6/mirror timeouts in `.github/workflows/build_android.yml`.

---

## 4. Active Bugs & Investigation Queue

### Priority 1 (Active): Overworld Fog Solid White Screen (`OVERWORLD_WEATHER_FOG`)
- **Where It Happens:** Foggy overworld maps such as **Route 210 North**.
- **Confirmed Behavior:**
  - Engine A (top main 3D screen) renders **solid white**; Engine B (floating PiP bottom screen) renders normally.
  - Using HM **Defog** immediately clears the white screen (`ov5_021D69B8` disables `BG2`, resets BG priorities, calls `G2_BlendNone()`, and calls `FogManager_ApplyParameters(..., FOG_PARAMETER_ENABLED, FALSE, ...)`).
- **Already Landed (`515c25503`..`b6624b3f4` — Did NOT Fully Resolve):**
  - Patched `G2x_SetBlendAlpha_`, `G2x_SetBlendBrightness_`, `G2x_SetBlendBrightnessExt_`, and `G2x_ChangeBlendBrightness_` in `subprojects/libntr/libraries/gx/src/g2.c` (`subprojects/packagefiles/libntr_g2_blend_regs.patch` wired into `subprojects/libntr.wrap`) under `#ifdef SDK_PORT` to write directly to `reg_G2_BLDALPHA` / `reg_G2_BLDY` instead of writing past `s_reg_G2_BLDCNT`.
  - Disabled hardware `G3X_SetFog` bit under `#ifndef SDK_BUILD_ARM` in `src/overlay005/fog_manager.c` while keeping `fogMan->enabled` set for Defog.
- **Exact Next Steps to Investigate:**
  1. **Verify `-DSDK_PORT` in `libntr`:** Inspect `subprojects/libntr/meson.build` (lines 110–165) to confirm whether `-DSDK_PORT` is actually defined when compiling `libntr` for `build_target=android`. If not, the `#ifdef SDK_PORT` blend-register fix in `g2.c` was never compiled in!
  2. **Audit G3 Fog Registers & Simulator Reads:** Check `src/overlay005/fog_manager.c` and `ov5_021D7308`–`ov5_021D7500` in `src/overlay005/ov5_021D5EB8.c` (lines 1880–2060), and verify `REG_FOG_TABLE_0_ADDR` in `subprojects/libntr/libraries/sim/src/g3_draw.cpp:414` on Android.
  3. **Audit `BG2` Alpha Blending Over `bg0as3d` in GLSL:** Inspect `subprojects/libntr/libraries/sim/src/shaders/g2_fragment.glsl` (lines 127–156). On Route 210 North (`ov5_021DAEC0`), fog `BG2` has priority `1` (drawn *above* 3D `BG0` at priority `2`), and `ov5_021D64E4` configures `BLDCNT` Target 1 = `BG2`, Target 2 = `BG0 | BD`. Trace how `bgc[2]` and `currentBgDrawn` blend when `BG0` is the 3D framebuffer (`bg0as3d != 0`).

### Priority 2 (Verify Status): Older Device Crash Signatures Seen in Mid-November Logs
If reported again during playtesting, check these earlier stack traces from historical tombstones:
- **`VeilstoneGymObjectRenderer_InitPunchingBagRenderer+180`** (`VeilstoneGym_DynamicMapFeaturesInit+312`): Null pointer write (`0x0`) when initializing Veilstone Gym punching bags.
- **`OverworldWild_Update+188` -> `OverworldWild_ClearAll` -> `MapObject_Delete+20`**: Null pointer read (`0x0`) when clearing visible overworld wild spawns.
- **`BattleSystem_MenuInput+644`**: `SEGV_ACCERR` (branch to non-executable address) in fallback Platinum battle menu input.
- **`ImDrawCmd::GetTexID()` Assertion (`imgui.hpp:4016`)**: Rare `SIGABRT` if an ImGui texture wasn't uploaded (`ImTextureData::SetTexID`) before draw submission.

# Repository Workflow Rules

- **Base ROM & Mod Setup:** Vanilla Pokémon Platinum patched with the Following Pokémon mod (not Renegade Platinum).
- **Context Refresh & Maintenance Cadence:** Every **8–10 turns** (and immediately at the start of any new chat, after context compaction, or when switching tasks), prompt the user to run `( cat AGENTS.md )` so these rules and the project state stay inside the active context window. Whenever a feature or fix is shipped, update `Last Worked On` and `All Included Features` in `AGENTS.md` before or alongside pushing.
- **Target Platform:** Android only (`android/`, `tools/android/build_apk.sh`, and GitHub Actions Android CI `.github/workflows/build_android.yml`). Do not suggest or run Linux/desktop builds.
- **Git & Deployment:** Every code change command must be self-contained and end by staging, committing, and pushing (`git add ... && git commit -m "..." && git push`) so GitHub Actions automatically builds a new APK (`VERSION_CODE=$((9100000 + RUN_NUMBER))`).
- **Terminal Safety:** Wrap multi-step shell scripts in a subshell `( ... )` and avoid `set -e` at the top-level interactive shell so a failed step never closes the Codespace terminal.
- **Gen 5 (`pokebw2`) Patches:** Keep `pokebw2` outside the repository (e.g. `/tmp/pokebw2` at commit `2c3961614d0fe1971ad0535e6edbbf3a6078e7b2`) and modify BW2 files strictly via `gen5/tools/amend_patch.py` into `gen5/patches/`.

- **Android CI `apt` Stability (`.github/workflows/build_android.yml`):** `Install Software` rewrites Ubuntu 24.04 sources to `http://azure.archive.ubuntu.com/ubuntu/` with IPv4 forced and `timeout-minutes: 25` so cold `apt` cache runs for `gcc-arm-none-eabi` do not time out at 10 minutes.

## Architecture & Technical Docs (`docs/architecture/`)

- **Master Technical Index:** [`docs/architecture/README.md`](docs/architecture/README.md) — Maps every custom fork feature (`docs/architecture/features/`) and core Platinum engine subsystem (`docs/architecture/core/`) to exact source files, functions, config flags, and thread-safety rules.

## Last Worked On

- **Canalave Library 3F TV Post-Fadeout Crash (In Progress — Handoff Notes):**
  - **Reproduction & Symptoms:**
    - In Canalave Library 3F (`res/field/scripts/scripts_canalave_library_3f.s:145–151`), after the TV broadcast cutscene finishes (`StartLibraryTV` -> `ReturnToField`), the game immediately crashes with `SIGSEGV` (`SEGV_MAPERR`) when executing `GetPlayerMapPos VAR_0x8004, VAR_0x8005`.
  - **Confirmed Crash Traces & Root Cause Analysis:**
    1. **Baseline crash (`ScrCmd_ReturnToField` returning `TRUE`):**
       - Faults at `MapObject_GetX+0` (`fault addr 0x64`, `x0 = 0x0000000000000000`), called from `PlayerAvatar_GetXPos(fieldSystem->playerAvatar)` inside `ScrCmd_GetPlayerMapPos` (`src/scrcmd.c:2514–2525`).
       - `fieldSystem->playerAvatar` is non-NULL, but `PlayerAvatar_GetMapObject(fieldSystem->playerAvatar)` (`src/player_avatar.c:383–389`) returns `playerAvatar->mapObject == NULL` (on Android `SDK_PORT`, `GF_ASSERT` is compiled out, so `NULL` flows directly into `MapObject_GetX`).
       - **Why `playerAvatar->mapObject` is `NULL`:** `ScrCmd_StartLibraryTV` (`src/scrcmd.c:3287–3293`) calls `FieldSystem_StartLibraryTV` -> `FieldSystem_StartChildProcess` (`src/field_system.c:140–145`), which sets `fieldSystem->runningFieldMap = FALSE` and sets `processManager->child = LibraryTV`. Because `HandleInputsEventsAndProcesses` (`src/field_system.c:218–227`) checks `if (processManager->parent) ... else if (processManager->child)`, `FieldMap_Main` runs first, sees `runningFieldMap == FALSE`, and triggers `FieldMap_Exit` (`src/overlay005/fieldmap.c:275–365`), calling `sub_02061BF0(fieldSystem->mapObjMan)` and `ov5_021ECC78(fieldSystem->mapObjMan)` to tear down map objects before `LibraryTV` runs. When `LibraryTV` exits, `ReturnToField` (`FieldTransition_StartMap`, `src/field_transition.c:82–92`) restarts `FieldMap` (`FieldMap_Init`), which sets `fieldSystem->runningFieldMap = TRUE` at `FIELD_MAP_INIT_STATE_DONE` without recreating `playerAvatar->mapObject` (note: `scripts_canalave_library_3f.s` does **not** call `WaitForTransition` before `StartLibraryTV`, unlike most other sub-app scripts).
    2. **Ruled-out hypothesis (`ScrCmd_ReturnToField` returning `FALSE`):**
       - Tested returning `FALSE` from `ScrCmd_ReturnToField` (reverted back to `TRUE`). In `ScriptContext_Run` (`src/field_script_context.c:91`), `return TRUE` is what yields (`break`) to `FieldTask_WaitUntilMapStarted`, while `return FALSE` immediately executes the next script command in the same frame. Returning `FALSE` caused `ScrCmd_GetPlayerMapPos` -> `ScriptContext_GetVarPointer` (`VAR_0x8004`) -> `FieldSystem_GetVarPointer` (`src/script_manager.c:452`) -> `FieldSystem_GetScriptMemberPtr` (`src/script_manager.c:408`) to call `FieldTask_GetEnv(fieldSystem->task)` while `FieldTask_WaitUntilMapStarted` (whose `env` is `NULL`) was still on top of the task stack, crashing at `FieldSystem_GetVarPointer+100` (`fault addr 0x0`).
  - **Additional Audit Findings (`playerAvatar` Lifecycle & Null-Safety Gaps):**
    1. **Only `playerAvatar->mapObject = mapObject` (`src/player_avatar.c:375`) writes to `playerAvatar->mapObject` inside `player_avatar.c`:** Neither `FieldMap_Exit` (`sub_02061BF0` / `ov5_021ECC78` in `src/overlay005/fieldmap.c:304–305`) nor `PlayerAvatar_Delete` (`src/player_avatar.c:121–127`) resets `playerAvatar->mapObject = NULL`, meaning `playerAvatar` is either overwritten/zeroed externally or left referencing a torn-down `MapObject` across `FieldMap_Exit` -> `FieldMap_Init` (`InitGraphicsAndManagers`, `src/overlay005/fieldmap.c:217`).
    2. **Dangling `fieldSystem->playerAvatar` pointer on teardown (`src/field_map_change.c:370–375`):** `FieldMapChange_DeleteObjects` calls `PlayerAvatar_Free(fieldSystem->playerAvatar)` (`Heap_Free`) without setting `fieldSystem->playerAvatar = NULL`. On native Android allocators, any transition hook or BGM query running before `FieldMapChange_CreateObjects`/`LoadObjects` (`src/field_map_change.c:353–389`) risks a use-after-free instead of a clean `NULL` check.
    3. **Eager `fieldSystem->playerAvatar` dereferences in `src/field_bgm.c`:**
       - `FieldBGM_GetEffective` (`src/field_bgm.c:140–141`) calls `PlayerAvatar_GetPlayerState(fieldSystem->playerAvatar)` immediately on entry (called during map transitions and `FieldMap_Init` in `src/overlay005/fieldmap.c:232, 437, 478`, `src/unk_0208BA78.c:323`, `src/unk_0203D1B8.c:1501`).
       - `FieldBGM_GetAltMusicForCyclingRoad` (`src/field_bgm.c:204–205`) reads `PlayerAvatar_GetXPos`/`GetZPos` *before* checking `if (mapHeaderID != MAP_HEADER_ROUTE_206)` at line `207`.
       - `FieldBGM_TryFadeOut` (`src/field_bgm.c:233–234`) reads `PlayerAvatar_GetPlayerState` *before* the `Sound_IsBGMFixed()` (`236`) and `bgmID == Sound_GetCurrentBGM1()` (`240`) early returns.
    4. **Repo-wide `playerAvatar` null-check audit:** Out of all `playerAvatar` dereferences in `src/`, only `src/unk_02069BE0.c:177` (`sub_02069CFC`) and `src/underground/secret_bases.c:1517` check `!= NULL`. Note that sibling mimic-NPC callbacks in `src/unk_02069BE0.c` (`164, 192, 202, 227, 642, 688`) do not guard `playerAvatar`.
    5. **Clean up leftover bogus files from an earlier session:** `CANALAVE_FIX_SUMMARY.md` (commit `4eb38b91a`), `fix_canalave_library_hybrid.patch` (0 bytes), `src/canalave_library_safety_hooks.asm` (0 bytes), and untracked `src/canalave_safety_checks.asm` / `src/field_transition_safety.c` contain non-functional 6502/placeholder text and are **not** wired into the build—safe to delete when cleaning up.
  - **Next Places to Inspect & Fix:**
    1. **`InitGraphicsAndManagers` (`src/overlay005/fieldmap.c:217`) & `ov5_021ECCBC` (`src/overlay005/ov5_021ECC20.c:51`):** Check how `MapObjectMan` and `playerAvatar` are re-initialized when `FieldMap_Init` runs after `ReturnToField` (compare `res/field/scripts/scripts_canalave_library_3f.s:144–154` with `scripts_jubilife_tv_3f.s:57–63` and `scripts_route_212_house.s:42–48`, and check whether `FieldMap_Init` needs to re-bind `playerAvatar->mapObject` or `scripts_canalave_library_3f.s` needs `WaitForTransition` before `StartLibraryTV`).
    2. **Defensive null-safety hardening:** Set `fieldSystem->playerAvatar = NULL;` after `PlayerAvatar_Free(fieldSystem->playerAvatar)` in `src/field_map_change.c:372`, and guard `fieldSystem->playerAvatar == NULL` in `src/field_bgm.c` (`FieldBGM_GetEffective`, `FieldBGM_GetAltMusicForCyclingRoad`, `FieldBGM_TryFadeOut`) and `ScrCmd_GetPlayerMapPos` (`src/scrcmd.c:2514`).
    3. **Note on Android Crash Log Capture:** On the test device (Samsung Galaxy Fold, no PC `adb`), native crash logs are captured via Termux + Shizuku using `$(command -v rish || echo ./rish) -c "logcat -b crash -d" > "/storage/emulated/0/Pokemon Platinum/Backup Saves/crash_library_tv.txt"`.
- **Overworld Fog Solid White Screen Bug (In Progress — Deferred to Fix a Higher-Priority Bug First):**
  - **Confirmed Symptoms & Isolation:**
    - Entering foggy overworld maps (such as Route 210 North, `OVERWORLD_WEATHER_FOG`) turns the Engine A (top screen) 3D viewport solid white.
    - Engine B (the PiP / sub-screen window) is completely unaffected, confirming the bug is isolated to Engine A's overworld fog pipeline rather than master brightness or sub-screen compositing.
    - Using the HM move **Defog** immediately clears the white screen (via `SystemFlag_CheckDefogActive` / `ov5_021D6868` transitioning the active weather task to `unk_BA6 = 5` -> `unk_BA2 = 5` -> `ov5_021D69B8`, which disables `BG2`, resets BG priorities, calls `G2_BlendNone()`, and calls `FogManager_ApplyParameters(..., FOG_PARAMETER_ENABLED, FALSE, ...)`).
  - **Changes Already Landed & Tested (`commit b6624b3f4`):**
    - **`subprojects/packagefiles/libntr_g2_blend_regs.patch` + `subprojects/libntr.wrap`:** Patched `G2x_SetBlendAlpha_`, `G2x_SetBlendBrightness_`, `G2x_SetBlendBrightnessExt_`, and `G2x_ChangeBlendBrightness_` in `subprojects/libntr/libraries/gx/src/g2.c` under `#ifdef SDK_PORT` to write directly to `reg_G2_BLDALPHA` / `reg_G2_BLDY` (and `reg_G2S_DB_*`) instead of writing out-of-bounds past `s_reg_G2_BLDCNT` in `.bss`.
    - **`src/overlay005/fog_manager.c`:** Suppressed `G3X_SetFog` enable bit on non-ARM (`#ifndef SDK_BUILD_ARM`) while preserving `fogMan->enabled` for Defog checks.
  - **What Was Ruled Out / Still Needs Investigation When Returning:**
    - Wiring `libntr_g2_blend_regs.patch` alone did **not** resolve the solid white screen on Route 210 North (`b6624b3f4` still reproduces it).
    - **Next places to inspect when resuming:**
      1. `subprojects/libntr/meson.build` (lines 110–165) to verify whether `-DSDK_PORT` is actually defined when compiling `libntr` on `build_target=android` (if `SDK_PORT` is not defined inside `libntr`, the `#ifdef SDK_PORT` branch in `g2.c` was skipped).
      2. `src/overlay005/fog_manager.c` (`FogManager_ApplyParameters`, `FogManager_ApplyColor`, `FogManager_ApplyDensityTable`) and `ov5_021D7308`–`ov5_021D7500` in `src/overlay005/ov5_021D5EB8.c` (lines 1880–2060) to verify all `G3X_SetFog` / `G3X_SetFogColor` / `G3X_SetFogTable` paths and confirm `REG_FOG_TABLE_0_ADDR` in `subprojects/libntr/libraries/sim/src/g3_draw.cpp:414` does not read unmapped DS I/O memory (`0x04000360`) on Android.
      3. `BG2` alpha blending interaction with `bg0as3d` in `subprojects/libntr/libraries/sim/src/shaders/g2_fragment.glsl` (lines 127–156): note that on Route 210 North (`ov5_021DAEC0`), `BG2` has priority 1 (above 3D `BG0` at priority 2), and `ov5_021D64E4` sets `BLDCNT` Target 1 = `BG2`, Target 2 = `BG0 | BD`. Check what `bgc[2]` and `currentBgDrawn` evaluate to in `g2_fragment.glsl` when `BG2` is drawn over `BG0` (`bg0as3d != 0`).

## All Included Features (Fork Inventory)

### Display, Layout & Android UI
- **Full-Screen Foldable & Phone Mode:** Top screen on the main display with a floating, rounded Picture-in-Picture (PiP) overlay for the bottom DS touch screen in the top-left corner (cyan bezel, adjustable overlay scale, quick hide/show toggle), plus classic split-screen mode with the **Gear** companion panel.
- **Pokémon-Themed UI & Controls:** Styled Settings and Gear screens, responsive touch overlays, left-stick-as-D-pad option, widescreen 3D rendering, 60 FPS, instant text, save slots/cloud sync, local multiplayer (ENet), and custom texture packs.

### Gen 5 Battles & Summary (`gen5/`, `src/port/gen5_*.{c,cpp}`)
- **Embedded BW2 Battle Engine & View:** Runs Platinum battles (wild, honey tree, roamer, scripted/legendary wild encounters, and trainer battles) inside Black 2 / White 2's engine (`pokebw2` compiled and symbol-prefixed as `bw2_*`, using the player's own B2/W2 USA ROM) with BW2 sound effects and cries over Platinum's music, plus BW2's Pokémon Summary and move-forget screens.
- **Full-Screen Touch Battle HUD (`src/port/touch_controls.cpp`, `src/port/gear_data.c`):**
  - 2x2 Action cards (`FIGHT`, `BAG`, `POKEMON`, `RUN`/`BACK`, plus Safari/Pal Park variants).
  - Type-colored Move cards with live PP counters, type pills, and effectiveness badges (`SUPER EFFECTIVE`, `NOT VERY EFFECTIVE`, `NO EFFECT`, accounting for Levitate, Wonder Guard, and Mold Breaker).
  - 2x2 Target selection grid highlighting single, spread, and self/side targets (red for foes, green for allies).
  - Two-card `YES` / `NO` prompt renderer for switch confirmations, move-forget prompts, and escape prompts.
  - `< [Poké Ball + Qty] >` quick-throw carousel with native NARC ball sprites and live Gen V weather badge.

### Overworld Following & Wild Encounters
- **Following Pokémon (`src/port/following_hack.c`):** C rewrite of Following Platinum's ARM follower logic using extracted overworld sprite tables.
- **Overworld Wild Pokémon (`Settings > Gameplay > Overworld wild Pokemon`):** Visible wild Pokémon dynamically spawn and roam in tall grass, caves, and water based on area encounter tables (time-of-day spawns, shiny variants, configurable density `1–6`, and water submerging with Flying-type exemption).

### Pokédex Completion & Encounter Tools
- **Gotta Catch 'Em All! Mode (`src/port/catch_em_all.c`, `Settings > Cheats`):** Injects Diamond/Pearl exclusives, Honey Tree/event species, GBA dual-slot encounters, post-Hall-of-Fame Sinnoh starters, and TID-independent Sinnoh + National Underground fossils into wild encounter tables and the Gear tracker.
- **Route Encounter & Dex Radar:** Unified live route encounter tracker in the **Gear** panel and **Area Radar** Pokétch app showing spawn percentages, method icons, caught badges, and route completion (synced with Swarms, Trophy Garden, Great Marsh, and *Gotta Catch 'Em All!*).
- **Encounter Toggles:** *No Wild Encounters* (disables random encounters while keeping visible overworld spawns active) and *Always Bite When Fishing* (guarantees fishing bites and bypasses Repel/Intimidate/Keen Eye).

### Gameplay & Quality-of-Life Upgrades
- **Portable PC Box Access:** Open the PC Box from the Start Menu (with restricted-map guardrails) or swap party/box Pokémon anywhere via the Gear panel.
- **HM & TM Freedom:** Use field moves (Cut, Fly, Surf, Strength, Rock Smash, Waterfall, Rock Climb, Defog) directly from compatible party Pokémon without teaching them (with optional badge bypass), *Universal TM/HM Compatibility*, and *Infinite TMs*.
- **Evolution, EXP & Timers:** *Trade Evolutions Without Trading* (Lv. 36+ for Kadabra/Haunter/Machoke/Graveler; level-up while holding required item for item-trade evolutions), *Modern EXP Share* (party-wide in both Platinum and Gen 5 battles), *Hard Level Cap* (next Gym Leader cap), *Instant Honey Trees*, reduced Berry growth timers, and *Game Clock Adjustment*.
- **Built-In Legendary Mystery Gift Events:** Triggers Darkrai (Member Card), Shaymin (Oak's Letter), Arceus (Azure Flute), and Rotom Forms (Secret Key) with prerequisite checks.

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

- **Canalave Library 3F TV Cutscene Crash Fix (`src/port/following_hack.c`, `src/unk_020655F4.c`, `include/unk_020655F4.h`, `src/overlay005/fieldmap.c`, `src/map_object.c`, `src/player_avatar.c`, `src/scrcmd.c`):**
  - **Root Cause:** `ScrCmd_FollowerAnimation` (`src/port/following_hack.c:1716`) called `MapObject_StartAnimation(follower, cmds)` and discarded the returned `SysTask *` without ever calling `MapObject_FinishAnimation`. Because `MovementAnimation_End` never stops its own `SysTask`, a permanent `MapObject_DoAnimation` `SysTask` remained registered in `SysTaskManager` with its `MoveAnimData` allocated at the top of `HEAP_ID_FIELD1` (`+0x187F90`). When `StartLibraryTV` ran `FieldMap_Exit` -> `Heap_Destroy(HEAP_ID_FIELD1)`, `LibraryTV` reused that memory from `HEAP_ID_APPLICATION`, overwriting `animData->state` to `1` (`MovementAnimation_CheckSet`) and `animData->mapObj` to `NULL`, causing `MapObject_DoAnimation` -> `MovementAnimation_CheckSet` -> `MapObject_CheckStatusFlag(NULL, MAP_OBJ_STATUS_0)` to `SIGSEGV` at `0x0`. (Note: the earlier Nov 20 `MapObject_GetX` log entry was an already-fixed Pastoria City bug.)
  - **Fix Shipped:**
    - Added `MapObject_SetAnimationAutoFinish(task, TRUE)` and wired it into `ScrCmd_FollowerAnimation` so follower hop tasks automatically call `MapObject_FinishAnimation(task)` (freeing `MoveAnimData`, clearing `MAP_OBJ_STATUS_4 | MAP_OBJ_STATUS_5`, and stopping the `SysTask`) as soon as `MOVEMENT_ACTION_END` is reached.
    - Added `MapObject_CancelAllAnimations()` in `src/unk_020655F4.c` and called it in `FieldMap_Exit` (`src/overlay005/fieldmap.c`) immediately before `Heap_Destroy(HEAP_ID_FIELD1)` so no `MoveAnimData` `SysTask` can ever outlive `HEAP_ID_FIELD1`.
    - Added defensive `NULL` guards in `MapObject_DoAnimation`, `MapObject_CheckStatus`, `MapObject_CheckStatusFlag`, `PlayerAvatar_EnsureMapObject`, `ScrCmd_RestoreCamera`, and `ScrCmd_GetPlayerMapPos`, and removed unused Canalave scratch files.
- **Overworld Fog Solid White Screen Bug (In Progress — Testing `libntr_g2_fog_blend.patch`):**
  - **Confirmed Symptoms & Isolation:**
    - Walking seamlessly north from Route 210 South into Route 210 North (`OVERWORLD_WEATHER_FOG = 14`, task `ov5_021DAEC0`) turns the Engine A (top screen) viewport solid white while Engine B (PiP / sub-screen) is unaffected.
    - Using HM **Defog** immediately clears the white screen via `ov5_021D69B8` (disables `BG2`, resets `BG2`/`BG0` priorities, calls `G2_BlendNone()`).
  - **Three-Part Audit Results:**
    1. **`-DSDK_PORT` & `libntr_g2_blend_regs.patch`:** Verified `-DSDK_PORT` is defined for `build_target=android` (`subprojects/libntr/meson.build:153-165, 202-210`) and `libntr_g2_blend_regs.patch` was cleanly applied and compiled in on CI (`b6624b3f4`).
    2. **3D Fog (`fog_manager.c`, `g3_draw.cpp`):** Ruled out — `FogManager_ApplyParameters` forces `G3X_SetFog(FALSE, ...)` under `#ifndef SDK_BUILD_ARM` and `REG_FOG_TABLE_0_ADDR` maps safely to `&s_reg_G3X_FOG_TOON_TABLE[0]`. That proves the white screen is the 2D `BG2` fog sheet (`weather_sys.narc` `0x33`/`0x2D`/`0x2F`, 100% pure white pixels) rendering opaquely.
    3. **`DrawEngine` Uniforms & `g2_fragment.glsl`:** Found `bgPriorities` and `bgOrder` in `sim_main.cpp:1229,1255` used `glUniform1iv` instead of `glProgramUniform1iv` (without re-binding `g2shaderProgramID` after secondary-screen/ImGui rendering), and `g2_fragment.glsl` dynamically indexed `bgc[bgNum]`, `bgc[currentBgDrawn]`, and `bgPriorities[bgNum]` on Android GLES 3.20.
  - **Fix & Diagnostic Wired (`subprojects/packagefiles/libntr_g2_fog_blend.patch` + `subprojects/libntr.wrap`):**
    - Switched `bgPriorities` and `bgOrder` uploads in `DrawEngine` (`sim_main.cpp`) to `glProgramUniform1iv(g2shaderProgramID, ...)` and ensured `glUseProgram(g2shaderProgramID)` before `DrawScreenQuad()`.
    - Eliminated dynamic `bgc[]`/`bgPriorities[]` array indexing in `g2_fragment.glsl`, used `color.rgb` in BG-over-BG alpha blending, and explicitly parenthesized `((bldcnt >> bgNum) & 1)`.
    - Added a throttled one-shot Android logcat diagnostic (`libntr-fog`) in `DrawEngine` when Engine A `BG2` is enabled at priority `1` to dump `dispcnt`, `bg0as3d`, `prio`, `order`, `bldcnt`, `bldalpha`, `bldy`, `wideMask`, and `disp3dcnt` on state changes.

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

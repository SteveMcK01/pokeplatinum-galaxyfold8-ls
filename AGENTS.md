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

### Current Handoff: Route 210 North / Foggy Overworld Solid White Screen (Android Port)

- **Goal:**
  - Fix a rendering bug on the Android port (`build_target = 'android'`) where walking into foggy overworld areas (specifically **Route 210 North**, `OVERWORLD_WEATHER_FOG` / weather `0xD` handled by `ov5_021DAEC0` in `src/overlay005/ov5_021D5EB8.c`) turns the top screen solid white (`#FFFFFF`) and heavily washes out the PiP bottom screen in white, while walking south back out of the route immediately restores normal rendering. (Dark-fog areas like Turnback Cave use the same fog system with `GX_RGB(0, 0, 0)`.)

- **What Has Been Tried So Far:**
  1. **Disabled hardware 3D fog (`G3X_SetFog`) on port builds (`src/overlay005/fog_manager.c`, commits `515c25503` & `9a27a2a34`):**
     - `FogManager_ApplyParameters` now passes `FALSE` to `G3X_SetFog` under `#ifndef SDK_BUILD_ARM` (while keeping `fogMan->enabled` accurate so `SystemFlag_CheckDefogActive` / Defog HM logic still works).
     - Verified via `subprojects/libntr/meson.build` that `build_target == 'android'` defines `-DSDK_PORT` and `-DSDK_BUILD_LINUX` / `-DSDK_BUILD_ANDROID` (and does **not** define `SDK_BUILD_ARM`).
     - **Result:** Did **not** fix the white screen on Route 210 North (`22763.mp4` recorded on commit `9a27a2a34` still shows the solid white top screen and washed-out PiP bottom screen upon stepping north onto Route 210 North).

- **Where Investigation Currently Stands (Root Cause Leads):**
  - Since `G3X_SetFog` is already disabled in `FogManager_ApplyParameters` and **both** the main top screen and the PiP bottom screen turn bright white simultaneously when crossing the map border into Route 210 North, the remaining white-out is **not** coming from 3D polygon fog (`u_fogEnable` in `sim_shaders.hpp`).
  - **Primary suspects to inspect next:**
    1. **2D `BG2` Fog Layer + Widescreen/Shader Blending (`subprojects/libntr/libraries/sim/src/shaders/g2_fragment.glsl` & `src/overlay005/ov5_021D5EB8.c`):**
       - `ov5_021DAEC0` enables `BG2` (`GXLayers_EngineAToggleLayers(GX_PLANEMASK_BG2, 1)`, priority `0` above `BG0` priority `1`, `charBase = 0x00000`, `screenBase = 0xe000`) and calls `ov5_021D64E4` → `G2_SetBlendAlpha(GX_BLEND_PLANEMASK_BG2, GX_BLEND_PLANEMASK_BG0 | GX_BLEND_PLANEMASK_BD, ev1, ev2)` ramping `ev1` from `0` to `9` (`ev2 = 16 - ev1`).
       - Because `charBase == 0x00000 && screenBase == 0xe000`, `sim_main.cpp:1318` sets `wideBgMask |= 1 << 2` (`G2SIM_DrawBGWide`). Check what `BG2` tile/palette data is uploaded in `ov5_021D6DCC` / `ov5_021D6E20` and how `g2_fragment.glsl` (`lines 125-156`) blends `bgc[2]` when `BG2` is drawn (`wideBgMask & 4`).
    2. **`MASTER_BRIGHT` / `BLDY` / `PortWeatherTask` (`src/overlay005/ov5_021D5EB8.c` & `subprojects/libntr/libraries/sim/src/sim_main.cpp:1416-1431`):**
       - Notice that in the test video, the **PiP bottom screen** (`isSub` / sub-engine) also becomes heavily washed out in white at the exact moment Route 210 North loads. Check `PortWeatherTask` in `src/overlay005/ov5_021D5EB8.c` (hooked at line `1199` under `#ifdef SDK_PORT`) and any `GX_SetMasterBrightness` / `GXS_SetMasterBrightness` / `G2_SetBlendBrightness` calls triggered on map/weather transitions (`src/field_map_change.c` or `ov5_021D5EB8.c`), as `g2_fragment.glsl:186` applies `inMasterBrightMode == 1` (`color.rgb += (1.0 - color.rgb) * inMasterBrightFactor`) as a full-screen whiteout.
    3. **Verify `G3X_SetFog` is not re-enabled elsewhere:**
       - Confirm no other call site writes `REG_G3X_DISP3DCNT_FME_MASK` or `G3X_SetFog` outside `FogManager_ApplyParameters` (already checked `src/overlay005/`; verify `subprojects/libntr` doesn't default `FME` on).

- **Immediate Next Steps for Next Session:**
  1. Inspect `PortWeatherTask` in `src/overlay005/ov5_021D5EB8.c` and check why both top and bottom screens whiten on Route 210 North entry.
  2. Temporarily test disabling `BG2` in `ov5_021DAEC0` (or clamping `BG2` alpha in `ov5_021D64E4` / `g2_fragment.glsl`) and/or logging `s_reg_GX_BLDCNT`, `s_reg_GX_BLDALPHA`, `s_reg_GX_MASTER_BRIGHT`, and `s_reg_GXS_DB_MASTER_BRIGHT` on Route 210 North to isolate whether `BG2` blending or `MASTER_BRIGHT` is saturating the frame.

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

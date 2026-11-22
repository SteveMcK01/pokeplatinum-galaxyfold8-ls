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

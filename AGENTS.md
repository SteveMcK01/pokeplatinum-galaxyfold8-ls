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
  - Fix a rendering bug on the Android port (`build_target = 'android'`) where walking into foggy overworld areas (specifically **Route 210 North**) turns the **top screen (Engine A) solid white (`#FFFFFF`)**, while walking south back out immediately restores normal rendering.

- **Key Clarification (PiP Bottom Screen):**
  - The PiP bottom screen is **not** independently turning white—its opacity automatically fades after 7 seconds without touch input, allowing the solid white Engine A canvas behind it to bleed through. The bug is isolated to **Engine A (top screen)**.

- **What Has Been Checked & Ruled Out:**
  1. **Hardware 3D Fog (`src/overlay005/fog_manager.c`, commits `515c25503` & `9a27a2a34`):**
     - `FogManager_ApplyParameters` already passes `FALSE` to `G3X_SetFog` under `#ifndef SDK_BUILD_ARM` (while keeping `fogMan->enabled` accurate for Defog HM checks).
     - Confirmed `build_target == 'android'` defines `-DSDK_PORT` and `-DSDK_BUILD_LINUX` / `-DSDK_BUILD_ANDROID` (not `SDK_BUILD_ARM`). Disabling `G3X_SetFog` did **not** stop the solid white screen.
  2. **`PortWeatherTask` (`src/overlay005/ov5_021D5EB8.c:104-110`):**
     - Verified `PortWeatherTask` only calls `FrameRate_SkipHalfRateTick()` for 60 FPS pacing and forwards to `portTask`; it does not touch `MASTER_BRIGHT` or blend registers.
  3. **`G2_SetBlendAlpha` Register Packing (`subprojects/libntr/libraries/gx/src/g2.c:44-55`):**
     - Verified `G2x_SetBlendAlpha_` packs `(G2_BLENDTYPE_ALPHA | plane1 | plane2 << 8) | ((ev1 | ev2 << 8) << 16)` cleanly for 64-bit `SDK_PORT` builds.
  4. **Fog Weather Tasks in `src/overlay005/ov5_021D5EB8.c`:**
     - `Unk_ov5_02201D78` maps weather entries `8` (`ov5_021D879C`, lines 3100–3179, heavy fog with `ev1` ramping `0 -> 16` / `ev2 = 16 - ev1` and `G2_SetBG2Priority(3)`) and `14` (`ov5_021DAEC0`, lines 5165–5240, standard fog with `ev1` ramping `0 -> 9` / `ev2 = 16 - ev1`). Both load BG resource `0x6` (`Unk_ov5_021F8D90[6]` = `{ 0x33, 0x2D, 0x2F }`) onto `BG2` and call `ov5_021D64E4(ev1, 16 - ev1)`.
     - Under widescreen (`src/port/widescreen.c:83-90`), `ov5_PortWeatherLayers` sets `wideBgs |= 1 << 2`, causing `sim_main.cpp:1343` to rasterize `BG2` via `G2SIM_DrawBGWide` (`subprojects/libntr/libraries/sim/src/sim_g2.cpp:306`).

- **Where Investigation Currently Stands (Exact Next Inspection):**
  - Why does Engine A turn **solid `#FFFFFF` white** even when `BG2` is placed at `priority 3` (behind 3D `BG0`) or blended at `ev1 = 9..16`? Two concrete places remain to check:
    1. **What `ov5_021D64E4` actually writes to `BLDCNT` / `BLDALPHA` and how `g2_fragment.glsl` (lines 125–210) composites `BG2` + 3D `BG0`:**
       - In `g2_fragment.glsl:127-155`, layers draw back-to-front (`priority 3 -> 0`). When `BG2` has `priority 3` (drawn first, `currentBgDrawn = 2`) and 3D `BG0` (`bgNum == 0 && bg0as3d != 0`) draws on top of it, `g2_fragment.glsl:131-134` overrides `alphaA = bgc[0].a; alphaB = 1.0 - alphaA;`—**only** if `BG0` is Target 1 (`(bldcnt >> 0) & 1`) and `BG2` is Target 2 (`(bldcnt >> (8 + 2)) & 1`). Conversely, if `BG2` is drawn *after* `BG0` (or `BG2` is Target 1 with `ev1 = 16, ev2 = 0`), `alphaA = 1.0, alphaB = 0.0` outputs 100% opaque `BG2` (`bgc[2]`).
    2. **What `G2SIM_DrawBGWide` (`sim_g2.cpp:306-360`) / `DrawBGLine_Text` outputs for `BG2` (and whether `MASTER_BRIGHT` / `BLDY` is active in `g2_fragment.glsl:160-210`):**
       - Check whether `BG2` tiles/palette (`0x33, 0x2D, 0x2F`) or `G2SIM_DrawBGWide` fill `BG2` with opaque white pixels (`alpha > 0.1`), or whether `inMasterBrightMode` / `BLDY` fade-to-white is triggered during the weather transition.

- **Immediate Next Steps for Next Session:**
  1. Run this single command to inspect `ov5_021D64E4`, the rest of `G2SIM_DrawBGWide`, and lines 135–210 of `g2_fragment.glsl`:
     ```bash
     (
       cd ~/pokeplatinum-galaxyfold8-ls
       echo "=== 1. ov5_021D64E4 in ov5_021D5EB8.c ==="
       grep -rn -n -C 20 "void ov5_021D64E4" src/overlay005/ov5_021D5EB8.c
       echo "=== 2. G2SIM_DrawBGWide body in sim_g2.cpp ==="
       nl -ba subprojects/libntr/libraries/sim/src/sim_g2.cpp | sed -n '306,365p'
       echo "=== 3. g2_fragment.glsl lines 135-210 ==="
       nl -ba subprojects/libntr/libraries/sim/src/shaders/g2_fragment.glsl | sed -n '135,210p'
     )
     ```
  2. Based on that output, check whether `G2SIM_DrawBGWide` is rendering `BG2` opaquely (e.g., wrong palette/alpha or `wideBgMask` issue) or `g2_fragment.glsl` is applying `ev1 = 16, ev2 = 0` / `MASTER_BRIGHT` white saturation, then patch and push (`git add ... && git commit -m "..." && git push`) to trigger the Android CI APK build.

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

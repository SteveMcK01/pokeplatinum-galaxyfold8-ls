# Repository Workflow Rules

- **Base ROM & Mod Setup:** Vanilla Pokémon Platinum patched with the Following Pokémon mod (not Renegade Platinum).
- **Context Refresh & Maintenance Cadence:** Every **8–10 turns** (and immediately at the start of any new chat, after context compaction, or when switching tasks), prompt the user to run `( cat AGENTS.md )` so these rules and the project state stay inside the active context window. Whenever a feature or fix is shipped, update `Last Worked On` and `All Included Features` in `AGENTS.md` before or alongside pushing.
- **Target Platform:** Android only (`android/`, `tools/android/build_apk.sh`, and GitHub Actions Android CI `.github/workflows/build_android.yml`). Do not suggest or run Linux/desktop builds.
- **Git & Deployment:** Every code change command must be self-contained and end by staging, committing, and pushing (`git add ... && git commit -m "..." && git push`) so GitHub Actions automatically builds a new APK (`VERSION_CODE=$((9100000 + RUN_NUMBER))`).
- **Terminal Safety:** Wrap multi-step shell scripts in a subshell `( ... )` and avoid `set -e` at the top-level interactive shell so a failed step never closes the Codespace terminal.
- **Gen 5 (`pokebw2`) Patches:** Keep `pokebw2` outside the repository (e.g. `/tmp/pokebw2` at commit `2c3961614d0fe1971ad0535e6edbbf3a6078e7b2`) and modify BW2 files strictly via `gen5/tools/amend_patch.py` into `gen5/patches/`.

- **Android CI `apt` Stability (`.github/workflows/build_android.yml`):** `Install Software` rewrites Ubuntu 24.04 sources to `http://azure.archive.ubuntu.com/ubuntu/` with IPv4 forced and `timeout-minutes: 25` so cold `apt` cache runs for `gcc-arm-none-eabi` do not time out at 10 minutes.

## Last Worked On

- **Gen 5 Effectiveness Type Offset, Core Yes/No Prompts & Move-Forget Screen:**
  - Offset BW2 `PokeType` IDs `>= 9` (`Fire`..`Dark`) by `+1` in `Gen5_GetBattleState` (`gen5/src/runtime.c`) to match Platinum's `TYPE_MYSTERY = 9`.
  - Checked `BTLV_INPUT_SCREEN_YES_NO` before `scd->numProcs == 0` in `Gen5_PortQueryBattleMenu` (`gen5/patches/src/ov169/btlv_scd.c.patch`) so `BtlvCore_ShowYesNo` prompts render `YES`/`NO` cards.
  - Called `Gen5_PortSetSubAppOpen(TRUE)` in `BtlvCore_StartForgetMoveSelect` (`gen5/patches/src/ov167/btlv_core.c.patch`) to open the DS bottom screen for move-forget selection.

- **Gen 5 Battle Target Highlighting & Yes/No Touch Overlay (`c270bd3b6`):**
  - Updated `gen5/patches/src/ov169/btlv_scd.c.patch` (`Gen5_PortQueryBattleMenu`) so spread/group/self/side moves (`scd->targets.selectCount == 0`, where `BtlvScd_GetTargetPos` outputs `BTL_POS_NONE`) resolve each slot's battle position via `ViewPosToBattlePos(scd->mainModule, sTargetButtonViewPos[kBw2Btn[t]])` and highlight every affected living battler instead of falling back to only slot `0`.
  - Updated `src/port/touch_controls.cpp` (`DrawFullScreenBattleOverlay`) to color-code `GEAR_MENU_TARGETS` cards (red for foes on `row == 0`, green for allies/self on `row == 1`) and added a two-card `GEAR_MENU_YES_NO` renderer (`menu.yes` -> `Gear_PressBattleButton(0)` at `(128, 56)`, `menu.no` -> `Gear_PressBattleButton(1)` at `(128, 104)`).
- **Overworld Water Spawn Submerging (`f079bf8b7`):** Kept Flying-type Pokémon floating above water while submerging Gyarados, Mantine, and Mantyke.
- **Full-Screen Gen 5 Battle Overlay & Poké Ball Carousel (`965927aec` – `4cb88a7d5`):** Added the transparent `< [Poké Ball + Qty] >` quick-throw carousel beside the player's status bar, bottom-right 2x2 action/move/target/Yes-No card panel, live Gen V weather pill, automatic dialogue wrapping, and PiP transition suppression.

## All Included Features (Fork Inventory)

### Display, Layout & Android UI
- **Full-Screen Foldable & Phone Mode:** Top screen on the main display with a floating, rounded Picture-in-Picture (PiP) overlay for the bottom DS touch screen in the top-left corner (cyan bezel, adjustable overlay scale, quick hide/show toggle), plus classic split-screen mode with the **Gear** companion panel.
- **Pokémon-Themed UI & Controls:** Styled Settings and Gear screens, responsive touch overlays, left-stick-as-D-pad option, widescreen 3D rendering, 60 FPS, instant text, save slots/cloud sync, local multiplayer (ENet), and custom texture packs.

### Gen 5 Battles & Summary (`gen5/`, `src/port/gen5_*.{c,cpp}`)
- **Embedded BW2 Battle Engine & View:** Runs Platinum battles inside Black 2 / White 2's engine (`pokebw2` compiled and symbol-prefixed as `bw2_*`, using the player's own B2/W2 USA ROM) with BW2 sound effects and cries over Platinum's music, plus BW2's Pokémon Summary and move-forget screens.
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
- **Evolution, EXP & Timers:** *Trade Evolutions Without Trading* (Lv. 36+ for Kadabra/Haunter/Machoke/Graveler; level-up while holding required item for item-trade evolutions), *Modern EXP Share* (party-wide), *Hard Level Cap* (next Gym Leader cap), *Instant Honey Trees*, reduced Berry growth timers, and *Game Clock Adjustment*.
- **Built-In Legendary Mystery Gift Events:** Triggers Darkrai (Member Card), Shaymin (Oak's Letter), Arceus (Azure Flute), and Rotom Forms (Secret Key) with prerequisite checks.

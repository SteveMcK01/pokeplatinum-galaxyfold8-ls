# Pokémon Platinum for Android (Foldable & Phone Edition)

A native port of Pokémon Platinum built specifically for **Android phones and foldables** (such as the Samsung Galaxy Fold series). It runs the decompiled game directly (no emulator), places the DS bottom touch screen on the lower half of the display alongside the **Gear** companion panel, and adds overworld wild encounters, Dex completion options, and modern quality-of-life upgrades.

This fork builds on [SilasMayes's pokeplatinum-thor](https://github.com/SilasMayes/pokeplatinum-thor) and [teton-dev's fork](https://github.com/teton-dev), which are built on [cybervisi0n's PC port](https://github.com/cybervisi0n/pokeplatinum), [libntr](https://github.com/cybervisi0n/libntr), and the [pret decompilation](https://github.com/pret/pokeplatinum). Please open issues here rather than on upstream repositories.

## ROM Requirement: Following Platinum

This repository contains **no ROM and no game assets**. On first launch, supply your own `.nds` dump of **Pokémon Platinum (USA)** (`CPUE`).

- **Designed for Following Platinum:** To enable **Following Pokémon** and **Overworld Wild Pokémon**, supply a **Following Platinum** patched `.nds` ROM on first launch. The port extracts Following Platinum's overworld sprite tables and uses them for both your follower and roaming wild encounters.
- **Optional Gen 5 Battles & Screens:** Point `Settings > Games > Gen 5 battles` to your own *Pokémon Black 2* or *White 2* (`.nds`) dump for animated Gen 5 battle arenas and Summary screens while preserving your Platinum party, moves, and stats.

## Setup Guide

1. **Install the App:** Download the latest `.apk` from the **Releases** page and install it on your Android device (enable installs from unknown sources if prompted).
2. **Import Pokémon Platinum (USA, Rev 1):** Launch the app and supply your own `.nds` dump of **Pokémon Platinum (USA, Rev 1)** (`CPUE`). *Note: The Rev 1 dump is required so the in-app patcher and asset extractor match cleanly.*
3. **Apply the Following Platinum Patch (In-App):** Download the **Following Platinum** patch file, then open **Settings > Games** inside the app to apply it. This enables **Following Pokémon** and extracts the overworld sprite tables needed for **Overworld Wild Pokémon**.
4. **Add Pokémon Black 2 or White 2 (Optional):** In **Settings > Games > Gen 5 battles**, import your own `.nds` dump of **Pokémon Black 2** or **Pokémon White 2** to enable animated 3D Gen 5 battle arenas and Summary screens while keeping your Platinum party, moves, and save data intact.
5. **Configure & Play:** Customize your screen layout, touch overlay, and gameplay toggles in **Settings**, then jump in and enjoy!

## What's New in This Fork

- **Full-Screen Gen V Battle Overlay & Poké Ball Quick-Throw Carousel:** Touch-friendly full-screen battle HUD featuring type-colored move cards with live PP counters and effectiveness badges, a live Gen V weather indicator, automatic dialogue text wrapping, and a transparent `< [Poké Ball + Qty] >` quick-throw carousel tucked beside the player's status bar using native Poké Ball sprites.
- **Route Encounter & Dex Radar (Gear & Pokétch App):** Unified route encounter tracker built into the **Gear** panel and the **Area Radar** Pokétch app—displaying species spawn rate percentages, encounter method icons, caught status badges, and a route completion counter synced live with Swarms, Trophy Garden, Great Marsh, and *Gotta Catch 'Em All!* encounters.
- **Overworld Wild Pokémon (`Settings > Gameplay > Overworld wild Pokemon`):** Visible wild Pokémon dynamically spawn and wander in tall grass, caves, and water using area encounter tables (including time-of-day spawns and shiny variants). Walk into one or press `A` to battle; water spawns have their lower third submerged and ground shadows hidden so they swim naturally, with configurable spawn density (`1` to `6`) in `Settings > Gameplay`.
- **Portable PC Box Access (Start Menu & Gear):** Access the in-game PC Box storage system directly from the Start Menu (with restricted-map guardrails) or swap party and box Pokémon anywhere in the overworld via the Gear panel.
- **Gotta Catch 'Em All! Mode (`Settings > Cheats`):** Adds missing Diamond/Pearl exclusives, Honey Tree/event Pokémon, GBA dual-slot species, and post-game Sinnoh starters directly to wild encounter tables and the Gear tracker (see the [Gotta Catch 'Em All! Encounter & Fossil Guide](#gotta-catch-em-all-encounter--fossil-guide) below).
- **Instant Honey Tree & Reduced Berry Timer:** Eliminates the multi-hour wait on slathered Honey Trees so encounters trigger immediately, and speeds up Berry growth timers.
- **Always Bite When Fishing Toggle:** Optional toggle that guarantees a Pokémon bites on every cast when fishing (and bypasses Repel as well as Intimidate / Keen Eye encounter blocks when active).
- **Infinite TMs:** Optional toggle to make TMs reusable like HMs without being consumed.
- **Trade Evolutions Without Trading:** Evolves trade-only Pokémon automatically on level-up (Lv. 36+ for Kadabra, Haunter, Machoke, and Graveler; any level-up while holding the required item for item-trade evolutions).
- **Full-Screen PiP Display Mode:** Adds a floating, rounded Picture-in-Picture (PiP) overlay for the bottom DS touch screen in the top-left corner with a cyan bezel, adjustable overlay scale, quick hide/show toggle, and full-screen main display support for unfolded foldables and phones.
- **Pokémon-Themed UI & Android Polish:** Styled the Settings and Gear companion screens with a cohesive Pokémon-inspired theme, refined split-screen touch controls, fixed Gen 5 battle move-map preservation, and resolved the Veilstone Gym trainer defeat crash.

## Gotta Catch 'Em All! Encounter & Fossil Guide

Enabling **Gotta Catch 'Em All!** in `Settings > Cheats` injects version exclusives, rare event/Honey Tree species, and national encounters directly into standard wild encounter tables—so they appear in normal grass/water encounters, as roaming **Overworld Wild Pokémon**, and in the **Gear Route Encounter & Dex Radar**.

### Sinnoh Dex & Diamond/Pearl Exclusives

| Pokémon | Location(s) | Method |
| :--- | :--- | :--- |
| **Murkrow** | Eterna Forest, Lost Tower (1F–5F) | Grass / Walking |
| **Misdreavus** | Eterna Forest, Lost Tower (1F–5F) | Grass / Walking |
| **Stunky** | Route 206, Route 214 | Grass |
| **Skuntank** | Route 214, Route 225 | Grass |
| **Glameow** | Route 218, Route 222 | Grass |
| **Purugly** | Route 222, Route 229 | Grass |
| **Munchlax** | Route 205 (North) | Grass |
| **Heracross** | Route 210 (South) | Grass |
| **Spiritomb** | Lost Tower (5F), Old Chateau | Walking |
| **Rotom** | Old Chateau | Walking |
| **Trapinch** | Route 228 | Grass |
| **Vibrava** | Route 228 | Grass |
| **Clamperl** | Route 219, Route 221 | Super Rod |

### Additional Regional & GBA-Slot Species

| Pokémon | Location(s) | Method |
| :--- | :--- | :--- |
| **Lotad** | Route 204 (South) | Grass |
| **Seedot** | Route 204 (South) | Grass |
| **Pineco** | Route 204 (North) | Grass |
| **Slowpoke** | Route 205 (South) | Grass |
| **Stantler** | Route 207 | Grass |
| **Zangoose** | Route 208 | Grass |
| **Seviper** | Route 208 | Grass |
| **Vulpix** | Route 209 | Grass |
| **Growlithe** | Route 209 | Grass |
| **Kecleon** | Route 210 (North) | Grass |
| **Teddiursa** | Route 211 (East) | Grass |
| **Ekans** | Route 212 (South) | Grass |
| **Poochyena** | Route 214 | Grass |
| **Sandshrew** | Wayward Cave (1F) | Cave |
| **Aron** | Fuego Ironworks (Outside) | Grass |
| **Lunatone** | Lake Verity | Grass |
| **Solrock** | Lake Valor | Grass |
| **Ursaring** | Lake Acuity | Grass |

### Post-Game Sinnoh Starters (After Entering the Hall of Fame)

Once you defeat the Champion and enter the Hall of Fame, the three Sinnoh starters unlock in the wild:

| Pokémon | Location(s) | Method |
| :--- | :--- | :--- |
| **Turtwig** | Eterna Forest | Grass |
| **Chimchar** | Route 227, Stark Mountain (Outside) | Grass |
| **Piplup** | Acuity Lakefront (Grass), Route 223 (Surf) | Grass / Surf |

### Fossils (Sinnoh Underground → Oreburgh Mining Museum)

All seven fossils can be excavated in the **Sinnoh Underground** and revived at the **Oreburgh Mining Museum** in Oreburgh City:

- **Sinnoh Fossils (Available Anytime):** Both the **Skull Fossil** (**Cranidos**) and **Armor Fossil** (**Shieldon**) share equal spawn weights in the Underground regardless of whether your Trainer ID is odd or even (removing vanilla Platinum's TID exclusivity).
- **National Fossils (Available After Obtaining the National Pokédex):** **Helix Fossil** (**Omanyte**), **Dome Fossil** (**Kabuto**), **Old Amber** (**Aerodactyl**), **Root Fossil** (**Lileep**), and **Claw Fossil** (**Anorith**).

## Core Port Features & Credits

- **Split-Screen & Gear:** Top screen on the main panel; DS touch screen and the **Gear** (damage calculator, turn order, area encounters, Pokédex, item map, PC sort, boss teams, shiny counters) on the lower half of the display, with optional widescreen, 60 FPS, instant text, fast egg hatching, save slots/sync, link play, and texture packs.
- **Modern EXP Share:** Optional party-wide EXP Share toggle that awards full EXP to battling Pokémon and half EXP to the rest of the non-fainted party.
- **Legendary Mystery Gift Events:** Built-in triggers for Nintendo's Mystery Gift events—gives the key item just like the Poké Mart delivery man and activates the event:
  - **Darkrai (Member Card):** Sleep at the Harbor Inn in Canalave City.
  - **Shaymin (Oak's Letter):** Visit the tablet on Route 224.
  - **Arceus (Azure Flute):** Climb to the top of Spear Pillar.
  - **Rotom Forms (Secret Key):** Enter the locked room of the Team Galactic building in Eterna City.
  - *Note:* Each displays any remaining prerequisites first (such as entering the Hall of Fame and obtaining the National Pokédex for Darkrai, Shaymin, and Arceus). The game checks when you arrive at the location, so leave and re-enter if you are already there.
- **Field Moves Without HMs:** Use Cut, Fly, Surf, Strength, Rock Smash, Waterfall, Rock Climb, and Defog directly from any compatible party Pokémon's menu without teaching the HM move (as long as the HM is in your bag and you have the required badge, with an optional toggle to bypass badge requirements).
- **Universal TM/HM Compatibility:** Option allowing any Pokémon to learn any TM or HM.
- **Hard Level Cap:** Prevents Pokémon from gaining EXP in battle once they reach the next Gym Leader's level cap.
- **Gen 5 battles (Settings > Battles > Gen 5 battles):** Replaces the platinum battle engine with the one from Black/White 2, this includes the sprites & battle screen.
- **Game Clock Adjustment:** Adjust the in-game clock offset directly in Settings for time-of-day encounters and evolutions without changing your device time.
- **No Wild Encounters Cheat:** Disables random encounters in grass, caves, and water while leaving visible **Overworld Wild Pokémon** active.
- **Build:** `make meson && ANDROID_NDK_HOME=/path/to/ndk ./tools/android/build_apk.sh`.
- **Credits:** [pret](https://github.com/pret/pokeplatinum), [cybervisi0n](https://github.com/cybervisi0n/pokeplatinum), [SilasMayes](https://github.com/SilasMayes/pokeplatinum-thor), [teton-dev](https://github.com/teton-dev) (catch-crash fix), **Following Platinum**, SDL, ENet, Dear ImGui, and Atkinson Hyperlegible. Pokémon is © Nintendo, Game Freak, and Creatures.

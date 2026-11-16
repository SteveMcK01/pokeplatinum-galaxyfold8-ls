# Pokémon Platinum for Android (Foldable & Phone Edition)

A native port of Pokémon Platinum built for **Android phones and foldables** (such as the Samsung Galaxy Fold series), with PC builds for Windows and Linux. It runs the decompiled game directly (no emulator), places the DS bottom touch screen on the lower half of the display alongside the **Gear** companion panel, and adds overworld wild encounters, Dex completion options, and modern quality-of-life upgrades.

This fork builds on [SilasMayes's pokeplatinum-thor](https://github.com/SilasMayes/pokeplatinum-thor) and [teton-dev's fork](https://github.com/teton-dev), which are built on [cybervisi0n's PC port](https://github.com/cybervisi0n/pokeplatinum), [libntr](https://github.com/cybervisi0n/libntr), and the [pret decompilation](https://github.com/pret/pokeplatinum). Please open issues here rather than on upstream repositories.

## ROM Requirement: Following Platinum

This repository contains **no ROM and no game assets**. On first launch, supply your own `.nds` dump of **Pokémon Platinum (USA)** (`CPUE`).

- **Designed for Following Platinum:** To enable **Following Pokémon** and **Overworld Wild Pokémon**, supply a **Following Platinum** patched `.nds` ROM on first launch. The port extracts Following Platinum's overworld sprite tables and uses them for both your follower and roaming wild encounters.
- **Optional Gen 5 Battles & Screens:** Point `Settings > Games > Gen 5 battles` to your own *Pokémon Black 2* or *White 2* (`.nds`) dump for animated Gen 5 battle arenas and Summary screens while preserving your Platinum party, moves, and stats.

## What's New in This Fork

- **Overworld Wild Pokémon (`Settings > Gameplay > Overworld wild Pokemon`):** Visible wild Pokémon dynamically spawn and wander in tall grass, caves, and water using area encounter tables (including time-of-day spawns and shiny variants). Walk into one or press `A` to battle; water spawns have their lower third submerged and ground shadows hidden so they swim naturally, with configurable spawn density (`1` to `6`) in `Settings > Gameplay`.
- **PC Anywhere:** Swap Pokémon in and out of your PC boxes and party from anywhere in the overworld without needing to visit a Pokémon Center.
- **Gotta Catch 'Em All! Mode:** Adds missing Diamond/Pearl exclusives and unobtainable Sinnoh Dex Pokémon (Murkrow, Misdreavus, Glameow, Stunky, Trapinch, Clamperl, fossils, and more) directly to wild encounters and the Gear tracker.
- **Infinite TMs & Universal TM/HM Compatibility:** Optional toggle to make TMs reusable like HMs without being consumed, plus an option allowing any Pokémon to learn any TM or HM.
- **Trade Evolutions Without Trading:** Evolves trade-only Pokémon automatically on level-up (Lv. 36+ for Kadabra, Haunter, Machoke, and Graveler; any level-up while holding the required item for item-trade evolutions).
- **Android & Foldable Polish:** Updated all Settings UI text for Android phones and foldables, refined split-screen touch overlay and Pokédex controls, and removed the legacy companion follower toggle to prevent conflicts with Following Platinum.
- **Gen 5 Battle Move Preservation Fix:** Fixed vanilla Platinum move-map initialization so party Pokémon retain all moves and PP across Gen 5 battles.

## Core Port Features & Credits

- **Split-Screen & Gear:** Top screen on the main panel; DS touch screen and the **Gear** (damage calculator, turn order, area encounters, Pokédex, item map, PC sort, boss teams, shiny counters) on the lower half of the display, with optional widescreen, 60 FPS, instant text, HM-free field moves, fast egg hatching, battle/exploration toggles, save slots/sync, link play, and texture packs.
- **Build:** `make meson && ANDROID_NDK_HOME=/path/to/ndk ./tools/android/build_apk.sh` (Android) or `./drun.sh make linux` (Linux).
- **Credits:** [pret](https://github.com/pret/pokeplatinum), [cybervisi0n](https://github.com/cybervisi0n/pokeplatinum), [SilasMayes](https://github.com/SilasMayes/pokeplatinum-thor), [teton-dev](https://github.com/teton-dev) (catch-crash fix), **Following Platinum**, SDL, ENet, Dear ImGui, and Atkinson Hyperlegible. Pokémon is © Nintendo, Game Freak, and Creatures.

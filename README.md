# Pokémon Platinum for Android (Foldable & Phone Edition)

A native port of Pokémon Platinum built for **Android phones and foldables** (such as the Samsung Galaxy Fold series), with PC builds for Windows and Linux. It runs the decompiled game directly (no emulator), places the DS bottom touch screen on the lower half of the display alongside the **Gear** companion panel, and adds overworld wild encounters and modern quality-of-life upgrades.

This fork is based on [SilasMayes's pokeplatinum-thor](https://github.com/SilasMayes/pokeplatinum-thor) and [teton-dev's fork](https://github.com/teton-dev), which build on [cybervisi0n's PC port](https://github.com/cybervisi0n/pokeplatinum), [libntr](https://github.com/cybervisi0n/libntr), and the [pret decompilation](https://github.com/pret/pokeplatinum). This is an independent fork—upstream authors are not involved, so please open questions and bug reports here rather than on upstream repositories.

## ROM Requirement: Following Platinum

This repository and its releases contain **no ROM and no game assets**. On first launch, the app asks for your own `.nds` dump of **Pokémon Platinum (USA)** (game code `CPUE`) and extracts the assets it needs.

- **Designed for Following Platinum:** To enable **Following Pokémon** (your lead Pokémon walking behind you in the overworld) and **Overworld Wild Pokémon**, supply a **Following Platinum** patched `.nds` ROM on first launch. The port detects the Following Platinum overworld sprite tables from your ROM and reuses them for both your follower and roaming wild encounters.
- **Optional Gen 5 Battles & Screens:** You can also point `Settings > Games > Gen 5 battles` to your own dump of *Pokémon Black 2* or *White 2* (`.nds`) to unlock animated Gen 5 battle arenas and Summary screens while preserving your Platinum party, moves, and stats.

## What's New in This Fork

- **Overworld Wild Pokémon (`Settings > Gameplay > Overworld wild Pokemon`):** Visible wild Pokémon dynamically spawn and wander in tall grass, caves, and water using the area's encounter tables (including time-of-day encounters and shiny variants). Walk into a wild Pokémon—or press `A` facing it—to start a battle. Water spawns have their lower third submerged and ground shadows hidden so they appear to swim naturally.
- **Configurable Spawn Density:** Adjust the maximum number of active roaming wild Pokémon per area (`1` to `6`) directly in `Settings > Gameplay`.
- **Android & Foldable Display Polish:** Replaced device-specific handheld references with phone/foldable layout defaults, refined the single-screen/split-screen touch overlay and Pokédex UI behavior, and removed the legacy companion follower toggle to prevent conflicts with Following Platinum's native followers.
- **Gen 5 Battle Move Preservation Fix:** Fixed vanilla Platinum move-table initialization so party Pokémon retain all moves and PP across Gen 5 engine battles.

## Core Port Features

- **Split-Screen Layout & Widescreen:** Top screen on the main panel, DS touch screen and the **Gear** on the lower half of the display, with optional widescreen on the overworld and in battles.
- **The Gear:** Battle menu with live damage ranges, KO indicators, and turn order; party, Day Care, and egg tracker; area wild encounter tables by method and time of day; Pokédex; route map with visible and hidden items; PC sorting; trainer card, level caps, and boss teams; and shiny-hunt counters.
- **Multiple Games Side by Side:** Play vanilla Platinum, Following Platinum, Renegade Platinum, or randomized ROMs side by side, each with isolated save slots.
- **Saves & Sync:** Four save slots, `.sav` import/export, autosave, and folder-based save sync across your Android device and PC via Syncthing or cloud storage.
- **Quality of Life:** 60 FPS overworld, instant text and HP/EXP bars, fast-forward, field moves without teaching HMs, *Use another Repel?* prompts, whole-party Exp. Share, game clock offset, and Nuzlocke helpers.
- **GBA Slot, Link Play & Texture Packs:** Dual-slot encounters and Pal Park support from a GBA save, local/network link battles and trades between devices, and custom texture pack loading.

## Build from Source

You need your own ROM to play what you build; building itself does not require one.

- **Android:** With the Android NDK, SDK, JDK 17, and Gradle installed (see [docs/thor/building.md](docs/thor/building.md)), run `make meson && ANDROID_NDK_HOME=/path/to/ndk ./tools/android/build_apk.sh`.
- **Linux:** With Docker, run `./drun.sh make linux`. Without Docker, install `nasm`, `enet`, `arm-none-eabi-gcc`, `ninja`, `flex`, and `bison`, then run `make linux`.
- **Windows:** Using MSYS2, run `Install_MSys2.ps1` once in PowerShell, then run `make win64` inside the MSYS2 shell.

## Credits

- [pret](https://github.com/pret/pokeplatinum): The decompilation of Pokémon Platinum.
- [cybervisi0n](https://github.com/cybervisi0n/pokeplatinum): The native PC port and `libntr` runtime libraries.
- [SilasMayes](https://github.com/SilasMayes/pokeplatinum-thor): The Android port, the Gear companion UI, Gen 5 battle integration, and Following Platinum support.
- [teton-dev](https://github.com/teton-dev): Upstream fork improvements, including the fix for crashing when catching new Pokémon.
- **Following Platinum & Drayano:** The Following Platinum overworld sprite patch and Renegade Platinum.
- [SDL](https://www.libsdl.org/), [ENet](http://enet.bespin.org/), [Dear ImGui](https://github.com/ocornut/imgui), and [Atkinson Hyperlegible](https://brailleinstitute.org/freefont) by the Braille Institute.

## License

The port code added in `src/port/`, `include/port/`, `android/`, and `tools/android/` is licensed under the MIT License; see [LICENSE](LICENSE). Bundled libraries retain their own licenses; see [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).

Pokémon is © Nintendo, Game Freak, and Creatures. This project is not affiliated with or endorsed by them. Please support the official games.

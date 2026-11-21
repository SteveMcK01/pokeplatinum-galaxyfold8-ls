# Pokédex Radar & Encounter Tools

Provides complete National Dex obtainability within a single Platinum save, a live route encounter radar in both the Gear panel and Pokétch, and convenience encounter toggles.

## 1. Gotta Catch 'Em All! Mode (`src/port/catch_em_all.c`, `include/port/catch_em_all.h`)

- **Config Flag:** `catchEmAll` (*Settings > Cheats* / *Gameplay*).
- **How It Works:**
  - Hooks directly into wild encounter table loading (`src/overlay006/wild_encounters.c`) and the Gear Dex builder (`src/port/gear_dex.c`) via:
    - `CatchEmAll_ApplyGrassSlots(mapHeaderID, slots)`
    - `CatchEmAll_ApplySurfSlots(mapHeaderID, slots)`
    - `CatchEmAll_ApplySuperRodSlots(mapHeaderID, slots)`
  - Replaces duplicate/filler slots on specific routes with Diamond/Pearl version exclusives (e.g., Misdreavus, Murkrow, Glameow, Stunky, Cranidos, Shieldon), Honey Tree/event species (Munchlax, Heracross, Combee, Burmy, Cherubi, Aipom), GBA dual-slot species, post-Hall-of-Fame Sinnoh starters (Turtwig, Chimchar, Piplup), and TID-independent Underground fossils—without altering the underlying ROM NARC files.

## 2. Unified Route Encounter & Dex Radar (`src/port/gear_dex.c`, `src/port/gear_data.c`)

- **Shared Data Backend:** Both the **Gear** companion's Area/Dex tab and the in-game **Area Radar** Pokétch app read from the same live encounter resolver in `src/port/gear_dex.c`.
- **Dynamic Sync:** Reflects active daily Swarms, Trophy Garden daily spawns, Great Marsh daily binoculars spawns, time-of-day replacements (Morning/Day/Night), GBA dual-slot cart setting (`src/port/gba_slot.c`), and *Gotta Catch 'Em All!* overrides.
- **Species Lookup (`Gear_GetSpecies`):** Provides base stats, types, abilities, egg groups, evolution chains (`Gear_EvolutionText`), TM compatibility, and exact route/method spawn percentages.

## 3. GBA Dual-Slot & Encounter Cheats

- **GBA Dual-Slot Emulation (`src/port/gba_slot.c`, `include/port/gba_slot.h`):** Emulates inserting Ruby, Sapphire, Emerald, FireRed, or LeafGreen into Slot-2 (`gbaSlotGame`) so dual-slot wild encounters appear naturally.
- **No Wild Encounters (`disableRandomEncounters` in `src/overlay006/wild_encounters.c:767`):** Suppresses random grass/cave/surf step encounters while leaving visible overworld wild Pokémon (`src/port/overworld_wild.c`) and scripted/static encounters active.
- **Always Bite When Fishing (`alwaysBiteFishing` in `src/overlay006/wild_encounters.c:404, 1156`):** Guarantees a fishing bite on every cast and bypasses Repel, Intimidate, and Keen Eye encounter blocks.

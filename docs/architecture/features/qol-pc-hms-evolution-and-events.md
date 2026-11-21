# QoL Upgrades, Portable PC, HMs, Evolution & Events

Covers gameplay convenience features, progression rules, timers, and built-in Mystery Gift event triggers.

## 1. Portable PC Box (`src/start_menu.c`, `src/port/gear_pc.c`, `src/port/save_editor.c`)

- **Start Menu Access (`src/start_menu.c:409`):** When `config->portablePC` is enabled, `HIDE_OPTION_PC` is cleared in `StartMenu_GetFieldHiddenOptions` unless the player is in a restricted state (no Pokémon/Bag access, invalid location, walking with a partner trainer, inside the Distortion World, or in a link/Wi-Fi session).
- **Gear Companion PC (`src/port/gear_pc.c`):** Allows viewing, sorting (`Gear_PCSort`), and swapping party/box Pokémon (`SaveEd_SwapWithPC`) directly from the Gear panel.

## 2. HM & TM Freedom (`src/port/hm_free.c`, `src/applications/`)

- **Field Moves Without Teaching HMs (`src/port/hm_free.c`):**
  - When `hmFree` is enabled, `HmFree_CanUse(party, move)` checks whether the corresponding HM item is in the player's bag and any non-Egg party Pokémon is present (`HmFree_SubstituteSlot` picks a compatible learner first, or the first non-Egg Pokémon).
  - Supports Cut, Fly, Surf, Strength, Defog, Rock Smash, Waterfall, and Rock Climb.
  - When `hmNoBadges` is enabled, `HmFree_NoBadges()` bypasses the gym badge checks for all 8 field moves.
- **Universal TM/HM Compatibility (`anyTM` in `src/port/game_platinum.c:84`):** Allows any Pokémon to learn any TM or HM.
- **Infinite / Reusable TMs (`reusableTMs` in `src/applications/bag/windows.c:333` & `src/applications/party_menu/callbacks.c:1118`):** Prevents TMs from being consumed when taught.

## 3. Evolution, EXP, Level Cap & Timers

- **Trade Evolutions Without Trading (`tradeEvos` in `src/pokemon.c:3787`):**
  - Checked during `Pokemon_GetEvolutionTargetSpecies` on level-up (`EVO_CLASS_BY_LEVEL`) when no standard level evolution triggers:
    - Pure `EVO_TRADE` species (Kadabra, Haunter, Machoke, Graveler) evolve at **Level 36+**.
    - `EVO_TRADE_WITH_HELD_ITEM` species (Onix + Metal Coat, Scyther + Metal Coat, Poliwhirl + King's Rock, Slowpoke + King's Rock, Seadra + Dragon Scale, Porygon + Up-Grade, Porygon2 + Dubious Disc, Rhydon + Protector, Electabuzz + Electirizer, Magmar + Magmarizer, Dusclops + Reaper Cloth, Clamperl + DeepSeaTooth/DeepSeaScale) evolve on **any level-up while holding the required item**.
- **Modern EXP Share (`expShareAll` in `src/port/game_platinum.c:85`):** Awards full battle EXP to active participants and 50% EXP to all other non-fainted party members.
- **Hard Level Cap (`hardLevelCap` + `Gear_LevelCap()`):** Enforced in both Platinum battles (`src/battle/battle_script.c:4991`) and Gen 5 battles (`src/port/gen5_battle.c:890`), preventing Pokémon from gaining EXP past the next Gym Leader's ace level.
- **Instant Honey Trees (`instantHoneyTrees` in `src/overlay005/honey_tree.c:148, 387`):** Eliminates the 6-hour real-time wait after slathering a Honey Tree.
- **Fast Berries (`fastBerries` in `src/berry_patches.c:108, 327`):** Compresses berry growth stages so planted berries mature in ~8 minutes.
- **Game Clock Offset (`clockOffsetMinutes`):** Shifts the RTC clock returned to the game (`SIM_SetClockOffset`) for time-of-day evolutions and spawns without changing the Android system clock.

## 4. Built-In Legendary Mystery Gift Events (`src/port/save_editor.c`, `include/port/save_editor.h`)

- `SaveEd_GetEvent(event, &out)` and `SaveEd_StartEvent(event)` inspect save flags and trigger the four Platinum Mystery Gift events safely (checking post-game/National Dex prerequisites and giving the key item + enabling the event script):
  - `SAVE_ED_EVENT_DARKRAI` (Member Card -> Newmoon Island)
  - `SAVE_ED_EVENT_SHAYMIN` (Oak's Letter -> Route 224 / Flower Paradise)
  - `SAVE_ED_EVENT_ARCEUS` (Azure Flute -> Spear Pillar / Hall of Origin)
  - `SAVE_ED_EVENT_ROTOM` (Secret Key -> Eterna Galactic Building appliances)

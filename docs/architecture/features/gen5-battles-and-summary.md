# Gen 5 Battles & Pokémon Summary (`gen5/`, `src/port/gen5_*.{c,cpp}`)

Runs Platinum battles, the Pokémon Summary screen, and move-forget selection inside Black 2 / White 2's C decompilation (`pokebw2`, compiled as a symbol-prefixed `bw2_*` static library) using assets loaded at runtime from the player's own **Pokémon Black 2 or White 2 (USA)** ROM.

## 1. Entry Points & Routing

### Battles
1. Platinum builds a `FieldBattleDTO` (`src/encounter.c`) and calls `FieldSystem_StartBattleProcess(fieldSystem, dto)` (`src/unk_0203D1B8.c:251`).
2. Under `#ifdef GEN5_BATTLES`, `Gen5Battle_Wants(dto)` (`src/port/gen5_battle.c:82`) decides whether to launch `gGen5BattleApplicationTemplate` or fall back to Platinum's `gBattleApplicationTemplate`:
   - Requires `SIM_Config_prj_GetConfig()->gen5Battles` and a validated B2/W2 ROM (`Gen5Rom_GameName() != NULL`).
   - **Wild battles (`BATTLE_TYPE_WILD_MON`, `BATTLE_TYPE_ROAMER`, `BATTLE_TYPE_AI_PARTNER`):** Allowed when `(dto->battleStatusMask & ~allowedWildFlags) == 0`, where `allowedWildFlags` includes `BATTLE_STATUS_HONEY_TREE`, `BATTLE_STATUS_NO_RUNNING`, `BATTLE_STATUS_LEGENDARY`, `BATTLE_STATUS_GIRATINA`, and `BATTLE_STATUS_DISTORTION`.
   - **Trainer battles (`BATTLE_TYPE_TRAINER`, `BATTLE_TYPE_TRAINER_DOUBLES`, `BATTLE_TYPE_TAG_DOUBLES`, `BATTLE_TYPE_TRAINER_WITH_AI_PARTNER`):** Allowed when `(dto->battleStatusMask & ~BATTLE_STATUS_FIRST_BATTLE) == 0`.
   - **Stays in Platinum:** Safari Zone, Pal Park, Battle Frontier, Link/Wi-Fi battles, and Catching Tutorial.
3. `Init()` (`src/port/gen5_battle.c:911`) converts `FieldBattleDTO` into `Gen5Battle` (parties, trainer art/messages, bag pockets, arena/terrain via `ArenaFromDTO`, clock time, Nuzlocke/Hard Level Cap rules), calls `Gen5Data_Prepare()` and `Gen5Data_PrepareGalacticArena()`, and starts BW2 via `Gen5_BattleBegin(&sBattle, Gen5Rom_Dir())`.
4. Each frame runs `Gen5_SetInput(...)`, `Gen5_SetWidescreen(...)`, and `Gen5_BattleFrame()` (`src/port/gen5_battle.c:1200`). On completion, `Finish()` writes party HP/PP/EXP/levels/moves/items, bag item deltas, money delta, and caught Pokémon back to `FieldBattleDTO`.

### Summary Screen
- Controlled by `SIM_Config_prj_GetConfig()->gen5Screens` (`src/port/gen5_summary.c`).
- Converts Platinum's `PokemonSummaryArgs` into `Gen5Summary` and runs BW2 overlay 207 (`PSTATUS_PROC_FUNCTIONS`) inside `Gen5_BattleFrame()`.

## 2. Key Translation & Compatibility Bridges

- **Type ID Offset (`gen5/src/runtime.c:609-610`):** Platinum defines `TYPE_MYSTERY = 9` between `TYPE_STEEL` (`8`) and `TYPE_FIRE` (`10`), whereas BW2 has no `TYPE_MYSTERY` (`Fire = 9` .. `Dark = 16`). `Gen5_GetBattleState` shifts BW2 `type1`/`type2 >= 9` by `+1` so UI type colors and effectiveness lookups match Platinum's `enum PokemonType`.
- **Distortion World & Galactic Arenas (`src/port/gen5_battle.c:753`):** `BACKGROUND_DISTORTION_WORLD`, `BATTLE_STATUS_DISTORTION`, `BATTLE_STATUS_GIRATINA`, and `TERRAIN_GIRATINA` map to `ARENA_BG_VOID` (`11`). Team Galactic battles use a custom arena (`src/port/gen5_galactic.c`) built from the Platinum ROM.
- **ROM Data Overrides (`src/port/gen5_data.c`, `src/port/gen5_rom.cpp`):** `Gen5Data_Prepare()` exports Platinum's personal stats, move info, icons, move names, move animations, and type chart (`Gen5_PortTypeChart`) into `Gen5_FilesOverride(...)` so modified tables carry into Gen 5 battles.
- **Menu & Move-Forget Bridges (`gen5/patches/`):**
  - `gen5/patches/src/ov169/btlv_scd.c.patch`: Implements `Gen5_PortQueryBattleMenu` (checking `BTLV_INPUT_SCREEN_YES_NO` before `scd->numProcs == 0`, and resolving spread/self/side target positions via `ViewPosToBattlePos`), `Gen5_PressBattleButton`, `Gen5_PortQueryBattleState`, and `Gen5_PortQueuePokeBall`.
  - `gen5/patches/src/ov167/btlv_core.c.patch`: Calls `Gen5_PortSetSubAppOpen(TRUE)` in `BtlvCore_StartForgetMoveSelect` so the DS PiP bottom screen opens during move-forget prompts.

## 3. Modifying `pokebw2` Code Safely

1. Keep `pokebw2` outside this repository (e.g. `/tmp/pokebw2` at commit `2c3961614d0fe1971ad0535e6edbbf3a6078e7b2`).
2. Edit the target file inside `/tmp/pokebw2` and generate/update the `.patch` file in `gen5/patches/` using:
   `python3 gen5/tools/amend_patch.py /tmp/pokebw2 <relative/path/in/pokebw2.c>`
3. Commit the updated `.patch` file in `gen5/patches/` (and any changes in `gen5/src/` or `gen5/api/`).

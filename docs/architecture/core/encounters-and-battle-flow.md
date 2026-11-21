# Core Engine: Encounters & Battle Flow

Reference for how Platinum constructs, dispatches, runs, and finalizes battles, and where Gen 5 and Touch/Gear overlays hook in.

## 1. Encounter Creation (`src/encounter.c`, `src/overlay006/wild_encounters.c`)

Battles allocate a `FieldBattleDTO` (`FieldBattleDTO_New` + `FieldBattleDTO_Init` in `src/field_battle_data_transfer.c`) and call `StartEncounter(...)` (`src/encounter.c:256`) or `Encounter_NewVsWild(...)`:

- **Grass / Cave / Surf / Fishing (`src/overlay006/wild_encounters.c`):** `BATTLE_TYPE_WILD_MON` (or `BATTLE_TYPE_AI_PARTNER` with a partner trainer).
- **Honey Tree (`Encounter_NewVsHoneyTree`, `src/encounter.c:563`):** `battleStatusMask |= BATTLE_STATUS_HONEY_TREE`.
- **Static / Legendary / Drifloon / Spiritomb / Rotom (`Encounter_NewVsSpeciesAtLevel`, `src/encounter.c:581`):** `BATTLE_TYPE_WILD_MON`; sets `BATTLE_STATUS_LEGENDARY` when `isLegendary == TRUE`.
- **Fateful Event (`Encounter_NewFatefulVsSpeciesAtLevel`, `src/encounter.c:603`):** Sets `MON_DATA_FATEFUL_ENCOUNTER = TRUE` and `BATTLE_STATUS_LEGENDARY`.
- **Distortion World Giratina (`Encounter_NewVsGiratinaOrigin`, `src/encounter.c:1008`):** `BATTLE_STATUS_LEGENDARY | BATTLE_STATUS_GIRATINA | BATTLE_STATUS_DISTORTION`, `terrain = TERRAIN_GIRATINA`.
- **Roamers (`src/encounter.c`):** `battleType = BATTLE_TYPE_ROAMER`.
- **Trainers (`Encounter_NewVsTrainer`, `src/encounter.c`):** `BATTLE_TYPE_TRAINER`, `BATTLE_TYPE_TRAINER_DOUBLES`, `BATTLE_TYPE_TAG_DOUBLES`, or `BATTLE_TYPE_TRAINER_WITH_AI_PARTNER`.

## 2. Engine Dispatch & Subscreen Input (`src/unk_0203D1B8.c:251`, `src/battle/`)

- `FieldSystem_StartBattleProcess` routes `dto` to `gGen5BattleApplicationTemplate` when `Gen5Battle_Wants(dto)` returns `TRUE`, otherwise `gBattleApplicationTemplate` (Safari Zone, Pal Park, Frontier, Catching Tutorial).
- On exit, `FieldTask_Encounter` calls `FieldBattleDTO_UpdateFieldSystem(dto, fieldSystem)` to commit party, items, money, and `resultMask`.
- **Platinum Action Menu Order (`sActionMenuTouchRects`, `src/battle/battle_subscreen.c:216`):** `0 = FIGHT`, `1 = BAG`, `2 = POKEMON`, `3 = RUN` (inverted `1`/`2` vs. BW2; translated in `Gear_PressBattleButton`).
- **Command Selection (`BattleControllerPlayer_CommandSelectionInput`, `src/battle/battle_controller_player.c:363`):** Runs on `SDLThread`. Queue external actions (like `BattleController_PortUseBall`) for `SDLThread` instead of calling `BattleSubscreen_PortClearMenu()` from the UI thread.

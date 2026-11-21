# Core Engine: Save Data, Party, Bag & Threading

Reference for accessing player state, persisting fork configuration, and keeping the ImGui UI thread decoupled from the DS/BW2 simulation thread.

## 1. Core Data Accessors (`FieldSystem` & `SaveData`)

- **Save Root (`SaveData *`):** Obtained via `fieldSystem->saveData` on the field or `SaveData_Ptr()` in port utilities.
- **Player Party (`Party *` & `Pokemon *`):**
  - `Party *party = SaveData_GetParty(saveData);`
  - `int count = Party_GetCurrentCount(party);`
  - `Pokemon *mon = Party_GetPokemonBySlotIndex(party, slot);`
  - Read/write fields via `Pokemon_GetValue(mon, MON_DATA_*, NULL)` and `Pokemon_SetValue(mon, MON_DATA_*, &val)` (`src/pokemon.c`), followed by `Pokemon_CalcLevelAndStats(mon)` after modifying IVs, EVs, nature, level, or species.
- **Bag (`Bag *`):**
  - `Bag *bag = SaveData_GetBag(saveData);`
  - `Bag_GetItemQuantity(bag, item, heapID)`, `Bag_TryAddItem(bag, item, count, heapID)`, `Bag_TryRemoveItem(bag, item, count, heapID)`.
- **Story Flags & Vars (`VarsFlags *`):**
  - `VarsFlags *vf = SaveData_GetVarsFlags(saveData);`
  - `VarsFlags_CheckFlag(vf, flagID)`, `VarsFlags_SetFlag(vf, flagID)`, `VarsFlags_ClearFlag(vf, flagID)`, and `VarsFlags_GetVar(vf, varID)`.
  - High-level helpers: `SystemFlag_CheckGameCompleted(vf)`, `SystemFlag_CheckHasPartner(vf)`, `Pokedex_GetNatDexFlag(SaveData_GetPokedex(saveData))`.

## 2. Fork Configuration (`include/port/sim_config_prj.h`, `src/port/sim_config_prj.c`)

- Global singleton: `SIM_Config_prj_type *cfg = SIM_Config_prj_GetConfig();`
- Adding a new setting requires updating four places:
  1. Add the field to `SIM_Config_prj_type` in `include/port/sim_config_prj.h`.
  2. Set its default in `SIM_Config_prj_LoadDefaults` (`src/port/sim_config_prj.c`).
  3. Parse and serialize its INI key in `SIM_Config_prj_LoadConfigFile` and `SIM_Config_prj_SaveConfigFile` (`src/port/sim_config_prj.c`).
  4. Expose its toggle/stepper in `src/port/gui_settings.cpp` (or `src/port/gui_cheats.cpp`).

## 3. Two-Thread Concurrency Rule (`SDLThread` vs. ImGui UI Thread)

The Android port runs two active threads:
1. **`SDLThread` (Game Engine Thread):** Executes Platinum's main loop, `SysTask`s, `FieldSystem`, `BattleSystem`, `Gen5_BattleFrame`, and DS heap allocations (`HEAP_ID_FIELD1`, `HEAP_ID_FIELD2`, `HEAP_ID_BATTLE`, `HEAPID_PORT_BATTLE`).
2. **UI / Render Thread:** Runs ImGui (`TouchControls_DrawOverlay`, `DrawFullScreenBattleOverlay`, Gear companion windows, Settings).

**Strict Thread-Safety Rules:**
- **Read-only snapshots from UI:** `Gear_GetBattle`, `Gear_GetBattleMenu`, `Gear_GetBattlePokeBalls`, and `OverworldWild_IsWaterSpawn` may safely read state for rendering (guarding pointers against `NULL` when transitions occur).
- **Never mutate DS heaps or state machines directly from UI:** Do not call `Heap_Alloc`, `Heap_Free`, `MapObject_Delete`, `BattleSubscreen_PortClearMenu`, or `Pokemon_SetValue` directly inside ImGui draw callbacks while the game is actively ticking. Instead, set an atomic/pending command variable (such as `sPortPress`, `sPendingBallItem`, or `Gen5_PortQueuePokeBall`) and let `SDLThread` execute the state transition on its next frame tick.

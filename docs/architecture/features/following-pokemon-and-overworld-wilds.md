# Following Pokémon & Overworld Wild Encounters

Manages visible Pokémon on the overworld: the player's lead party Pokémon following behind them and wild Pokémon dynamically spawning and roaming in tall grass, caves, and water.

## 1. Following Pokémon (`src/port/following_hack.c`, `src/port/follower.c`)

This fork runs **vanilla Pokémon Platinum patched with the Following Pokémon mod**.

- **Following Mod C Rewrite (`src/port/following_hack.c`, `include/port/following_hack.h`):**
  - At startup, `FollowingHack_Init()` loads the Following Platinum ARM data blob from `weather_sys.narc` member 65 (`FOLLOWING_HACK_BASE = 0x023C8000`) and extends Platinum's overworld sprite renderer, texture, animation, and render-detail tables with the mod's 566 Pokémon graphics (`IDs 276..841`).
  - Because Android ARM64 cannot execute DS Thumb code directly, `following_hack.c` reimplements the follower lifecycle in C:
    - Uses `FOLLOWER_LOCAL_ID = 0xF4` (and script alias `0xFD` via `FollowingHack_ScriptObject`).
    - Movement types `FOLLOWER_MOVEMENT_WALK` (`0x44`) and `FOLLOWER_MOVEMENT_STAND` (`0x45`).
    - Hooked into `FieldMapChange_CreateObjects` (`FollowingHack_MapLoaded`), `FieldMap_ChangeZone` (`FollowingHack_MapConnected`), `Player_SetMoveState` (`FollowingHack_PlayerStep`), collision checks (`FollowingHack_ObjectAt`), and Poké Ball release/return field effects.
- **Unpatched Fallback (`src/port/follower.c`):** Inactive while `FollowingHack_IsActive()` is `TRUE`; provides a basic follower (`FOLLOWER_LOCAL_ID = 0xF3`) if a plain unpatched Platinum ROM is ever loaded.
- **Script Object Lookup Safety (`src/scrcmd.c`):** All object-lookup script commands (`ScrCmd_Unused_06A` / `0x006A` `GetObjectPos`, `LockObject`, `ReleaseObject`, `ShowObject`, `HideObject`, `SetPosition`, `SetMovementType`, `ShakeObject`, `FlickerObject`, etc.) route through `GetLocalMapObjByIndex()` and guard against `NULL` pointers so missing local IDs (e.g., `0x2e` in Pastoria City) or follower IDs never dereference `NULL` on ARM64.

## 2. Overworld Wild Pokémon (`src/port/overworld_wild.c`, `include/port/overworld_wild.h`)

- **Config Flags:** `overworldWildPokemon` (`BOOL`) and `overworldWildDensity` (`1..6`, default `3`).
- **Transient Object Pool:**
  - Spawns up to `OVERWORLD_WILD_MAX_ACTIVE` (`6`) `MapObject` instances using local IDs `0xE0..0xE5` (`OVERWORLD_WILD_BASE_LOCAL_ID`) and Pokémon graphics IDs from `FollowingHack_GetSpeciesObjGfx(...)`.
  - Excluded from save serialization via `OverworldWild_IsTransientWildObject(mapObj)` in `src/map_object.c`.
- **Spawn & Movement Lifecycle (`OverworldWild_Update`):**
  - Samples candidate tiles `3..10` tiles from the player (`IsValidWildEncounterTile`), checks area grass/cave/surf encounter tables (respecting time of day, *Gotta Catch 'Em All!*, and `shinyOdds`), and despawns slots beyond `15` tiles (`OVERWORLD_WILD_DESPAWN_DIST`) after a grace period.
  - **Water Submerging (`ShouldSubmergeWaterSpecies` in `src/port/overworld_wild.c:437`):** Water spawns sink slightly into the water surface for immersion, except Flying-type species (e.g., Gyarados, Mantine, Mantyke, Wingull, Pelipper) which stay elevated above the water.
  - **Encounter Trigger:** Walking into a wild Pokémon (`OverworldWild_TryTouchEncounter`) or pressing A (`OverworldWild_TryInteract`) snapshots active slots (`sSavedBattleSlots`), creates a scripted `FieldBattleDTO`, starts the battle, and restores surviving overworld spawns once the battle finishes.

# Core Engine: Scripts & Map Objects

Reference for Platinum's field script VM, overworld `MapObjectManager`, map loading/transitions, and 64-bit Android pointer safety.

## 1. Field Script VM (`src/scrcmd.c`, `include/data/scripts/scrcmd.h`)

- Scripts execute via `ScriptContext` (`ctx->fieldSystem`, `ctx->task`), dispatching opcodes through `gScriptCmdTable` (`src/scrcmd.c`).
- **Following Pokémon Mod Opcodes:**
  - Commands `840` and `844..860` are implemented in `src/port/following_hack.c` (`ScrCmd_ObjectBall`, `ScrCmd_FirstBattler`, etc.).
  - Vanilla command `0x006A` (`ScrCmd_Unused_06A` in `src/scrcmd.c:2522`) is used by Following Platinum scripts to read an object's `(X, Z)` tile coordinates (`GetObjectPos localID, outX, outZ`).
- **Local ID Resolution (`GetLocalMapObjByIndex` in `src/scrcmd.c:2132`):**
  - `LOCALID_FOLLOWER` (`0xFF`) / `0xF4` / `0xFD`: Resolves via `FollowingHack_ScriptObject` or `sFollower`.
  - `LOCALID_CAMERA` (`0xF1`): Resolves `*SCRIPT_MANAGER_CAMERA_OBJECT`.
  - Standard IDs (`0..254`): Calls `MapObjMan_LocalMapObjByIndex(fieldSystem->mapObjMan, localID)`.

## 2. `MapObjectManager` & Map Transitions

- **Object Pool (`src/field_map_change.c:355`):** `MapObjectMan_New(fieldSystem, 64, 5)` allocates a fixed array of **64** `MapObject` slots per map zone (`0` = player, `1..N` = map header `ObjectEvent`s, plus follower `0xF4` and transient overworld wilds `0xE0..0xE5`).
- **Map Load & Zone Crossing Hooks:**
  - **Warp / Map Load (`FieldMapChange_CreateObjects` in `src/field_map_change.c:353`):** Spawns `PlayerAvatar`, loads map header objects via `sub_0203A418(fieldSystem)` (`src/map_header_data.c:51`), then calls `FollowingHack_MapLoaded(fieldSystem)`.
  - **Seamless Border Crossing (`FieldMap_ChangeZone` in `src/overlay005/fieldmap.c:415`):** Updates `mapHeaderID`, reconciles objects across the border via `sub_0206184C`, calls `FollowingHack_MapConnected(fieldSystem)`, then spawns new map objects via `sub_0203A418(fieldSystem)`.

## 3. Android ARM64 `NULL` Pointer & `GF_ASSERT` Rules

- On original Nintendo DS ARM9 hardware, address `0x00000000` is readable ITCM memory, so vanilla scripts that pass an unspawned `localID` to `MapObjMan_LocalMapObjByIndex` (receiving `NULL`) read a harmless dummy value from `NULL + offset`.
- On **Android ARM64**, page zero (`0x00000000..0x0000FFFF`) is unmapped, and `GF_ASSERT(FALSE)` is a non-halting log macro when `enableAsserts` is off.
- **Rule:** Every `MapObject *` lookup in `src/scrcmd.c` or `src/port/` must use `GetLocalMapObjByIndex` (when resolving script local IDs) and return safely if `mapObj == NULL` before calling any `MapObject_*` accessor.

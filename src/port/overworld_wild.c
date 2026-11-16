#include "port/overworld_wild.h"
#include "port/following_hack.h"
#include "port/sim_config_prj.h"
#include "constants/map_object.h"
#include "generated/movement_types.h"
#include "save_player.h"
#include "trainer_info.h"
#include "constants/battle.h"

#include <string.h>

#include "constants/heap.h"
#include "constants/pokemon.h"
#include "generated/pokemon_data_params.h"

#include "encounter.h"
#include "field/field_system.h"
#include "field_battle_data_transfer.h"
#include "map_header.h"
#include "map_object.h"
#include "map_tile_behavior.h"
#include "math_util.h"
#include "overlay006/wild_encounters.h"
#include "party.h"
#include "player_avatar.h"
#include "pokemon.h"
#include "savedata.h"
#include "system.h"
#include "terrain_collision_manager.h"

typedef struct OverworldWildSlot {
    BOOL active;
    u16 localId;
    u16 species;
    u8 level;
    u8 form;
    u8 gender;
    BOOL isShiny;
    BOOL isWaterSpawn;
    MapObject *mapObj;
} OverworldWildSlot;

static OverworldWildSlot sWildSlots[OVERWORLD_WILD_MAX_ACTIVE];
static u16 sRespawnCooldownTimer = 0;
static enum MapHeaderID sLastMapHeaderID = (enum MapHeaderID)-1;
static BOOL sNeedsAreaSpawn = TRUE;

static inline int AbsDiff(int a, int b)
{
    return (a > b) ? (a - b) : (b - a);
}

static int GetTargetDensity(void)
{
    SIM_Config_prj_type *cfg = SIM_Config_prj_GetConfig();
    if (!cfg->overworldWildPokemon) {
        return 0;
    }
    int d = cfg->overworldWildDensity;
    if (d < 1) {
        return OVERWORLD_WILD_DEFAULT_DENSITY;
    }
    if (d > OVERWORLD_WILD_MAX_ACTIVE) {
        return OVERWORLD_WILD_MAX_ACTIVE;
    }
    return d;
}

BOOL OverworldWild_IsTransientWildObject(const MapObject *mapObj)
{
    if (mapObj == NULL) {
        return FALSE;
    }
    u32 localId = MapObject_GetLocalID(mapObj);
    return (localId >= OVERWORLD_WILD_BASE_LOCAL_ID &&
            localId < (OVERWORLD_WILD_BASE_LOCAL_ID + OVERWORLD_WILD_MAX_ACTIVE));
}

static void DeleteTransientObjectWithLocalID(MapObjectManager *mapObjMan, u32 targetLocalId)
{
    if (mapObjMan == NULL) {
        return;
    }
    MapObject *obj = NULL;
    int index = 0;
    while (MapObjectMan_FindObjectWithStatus(mapObjMan, &obj, &index, MAP_OBJ_STATUS_0)) {
        if (MapObject_GetLocalID(obj) == targetLocalId) {
            MapObject_Delete(obj);
            return;
        }
    }
}

static void DeleteOrphanedTransientObjects(MapObjectManager *mapObjMan)
{
    if (mapObjMan == NULL) {
        return;
    }
    for (int i = 0; i < OVERWORLD_WILD_MAX_ACTIVE; i++) {
        DeleteTransientObjectWithLocalID(mapObjMan, OVERWORLD_WILD_BASE_LOCAL_ID + i);
    }
}

static void DespawnSlot(int idx)
{
    if (sWildSlots[idx].mapObj != NULL) {
        MapObject_Delete(sWildSlots[idx].mapObj);
        sWildSlots[idx].mapObj = NULL;
    }
    sWildSlots[idx].active = FALSE;
}

void OverworldWild_ClearAll(FieldSystem *fieldSystem)
{
    for (int i = 0; i < OVERWORLD_WILD_MAX_ACTIVE; i++) {
        DespawnSlot(i);
    }
    if (fieldSystem != NULL) {
        DeleteOrphanedTransientObjects(fieldSystem->mapObjMan);
    }
    sRespawnCooldownTimer = OVERWORLD_WILD_RESPAWN_COOLDOWN;
    sNeedsAreaSpawn = TRUE;
}

void OverworldWild_Init(FieldSystem *fieldSystem)
{
    if (fieldSystem != NULL) {
        DeleteOrphanedTransientObjects(fieldSystem->mapObjMan);
    }
    memset(sWildSlots, 0, sizeof(sWildSlots));
    sRespawnCooldownTimer = OVERWORLD_WILD_RESPAWN_COOLDOWN;
    sLastMapHeaderID = fieldSystem ? fieldSystem->location->mapHeaderID : (enum MapHeaderID)-1;
    sNeedsAreaSpawn = TRUE;
}

static BOOL IsTileOccupiedOrTooClose(const MapObjectManager *mapObjMan, int x, int z)
{
    MapObject *obj = NULL;
    int index = 0;
    while (MapObjectMan_FindObjectWithStatus(mapObjMan, &obj, &index, MAP_OBJ_STATUS_0)) {
        int ox = MapObject_GetX(obj);
        int oz = MapObject_GetZ(obj);
        if (ox == x && oz == z) {
            return TRUE;
        }
        if (MapObject_GetXPrev(obj) == x && MapObject_GetZPrev(obj) == z) {
            return TRUE;
        }
        if (OverworldWild_IsTransientWildObject(obj)) {
            if ((AbsDiff(ox, x) + AbsDiff(oz, z)) < OVERWORLD_WILD_MIN_SPACING) {
                return TRUE;
            }
        }
    }
    return FALSE;
}

static BOOL IsValidWildEncounterTile(const FieldSystem *fieldSystem, int x, int z, BOOL *outIsWater)
{
    *outIsWater = FALSE;

    if (TerrainCollisionManager_CheckCollision(fieldSystem, x, z)) {
        return FALSE;
    }

    u8 behavior = TerrainCollisionManager_GetTileBehavior(fieldSystem, x, z);
    if (!TileBehavior_HasEncounters(behavior)) {
        return FALSE;
    }

    if (TileBehavior_IsSurfable(behavior)) {
        *outIsWater = TRUE;
    }

    return TRUE;
}

static BOOL TrySpawnOneWildPokemon(FieldSystem *fieldSystem, int maxAttempts)
{
    if (!FollowingHack_IsActive()) {
        return FALSE;
    }
    if (!MapHeader_HasWildEncounters(fieldSystem->location->mapHeaderID)) {
        return FALSE;
    }

    int targetDensity = GetTargetDensity();
    int activeCount = 0;
    int freeSlot = -1;
    for (int i = 0; i < OVERWORLD_WILD_MAX_ACTIVE; i++) {
        if (sWildSlots[i].active) {
            activeCount++;
        } else if (freeSlot == -1) {
            freeSlot = i;
        }
    }
    if (activeCount >= targetDensity || freeSlot == -1) {
        return FALSE;
    }

    int playerX = PlayerAvatar_GetXPos(fieldSystem->playerAvatar);
    int playerZ = PlayerAvatar_GetZPos(fieldSystem->playerAvatar);

    for (int attempt = 0; attempt < maxAttempts; attempt++) {
        int dx = (int)(LCRNG_Next() % (OVERWORLD_WILD_SPAWN_MAX_DIST * 2 + 1)) - OVERWORLD_WILD_SPAWN_MAX_DIST;
        int dz = (int)(LCRNG_Next() % (OVERWORLD_WILD_SPAWN_MAX_DIST * 2 + 1)) - OVERWORLD_WILD_SPAWN_MAX_DIST;
        int dist = AbsDiff(dx, 0) + AbsDiff(dz, 0);

        if (dist < OVERWORLD_WILD_SPAWN_MIN_DIST || dist > OVERWORLD_WILD_SPAWN_MAX_DIST) {
            continue;
        }

        int targetX = playerX + dx;
        int targetZ = playerZ + dz;
        BOOL isWater = FALSE;

        if (!IsValidWildEncounterTile(fieldSystem, targetX, targetZ, &isWater)) {
            continue;
        }

        if (IsTileOccupiedOrTooClose(fieldSystem->mapObjMan, targetX, targetZ)) {
            continue;
        }

        u8 behavior = TerrainCollisionManager_GetTileBehavior(fieldSystem, targetX, targetZ);
        u16 species = 0;
        u8 level = 1;
        u8 form = 0;
        BOOL isShiny = FALSE;

        if (!WildEncounters_TryRollOverworldSpecies(
                fieldSystem, behavior, isWater, &species, &level, &form, &isShiny)) {
            continue;
        }

        u8 gender = (LCRNG_Next() & 1) ? GENDER_FEMALE : GENDER_MALE;
        u16 gfxId = FollowingHack_GetSpeciesObjGfx(species, form, gender);
        if (gfxId == 0xFFFF) {
            continue;
        }

        u16 localId = OVERWORLD_WILD_BASE_LOCAL_ID + freeSlot;
        DeleteTransientObjectWithLocalID(fieldSystem->mapObjMan, localId);

        MapObject *wildObj = MapObjectMan_AddMapObject(
            fieldSystem->mapObjMan,
            targetX,
            targetZ,
            LCRNG_Next() % 4,
            gfxId,
            MOVEMENT_TYPE_WANDER_AROUND,
            fieldSystem->location->mapHeaderID
        );

        if (wildObj == NULL) {
            return FALSE;
        }

        MapObject_SetLocalID(wildObj, localId);
        MapObject_SetTrainerType(wildObj, 0);
        MapObject_SetScript(wildObj, 0);
        MapObject_SetFlag(wildObj, 0);
        MapObject_SetMovementRangeX(wildObj, 2);
        MapObject_SetMovementRangeZ(wildObj, 2);
        MapObject_SetDataAt(wildObj, isShiny ? 0x80 : 0, 2);

        sWildSlots[freeSlot].active = TRUE;
        sWildSlots[freeSlot].localId = localId;
        sWildSlots[freeSlot].species = species;
        sWildSlots[freeSlot].level = level;
        sWildSlots[freeSlot].form = form;
        sWildSlots[freeSlot].gender = gender;
        sWildSlots[freeSlot].isShiny = isShiny;
        sWildSlots[freeSlot].isWaterSpawn = isWater;
        sWildSlots[freeSlot].mapObj = wildObj;
        return TRUE;
    }

    return FALSE;
}

static void PopulateAreaBatch(FieldSystem *fieldSystem)
{
    int targetDensity = GetTargetDensity();
    for (int i = 0; i < targetDensity; i++) {
        TrySpawnOneWildPokemon(fieldSystem, 28);
    }
}

static void TriggerSlotEncounter(FieldSystem *fieldSystem, int slotIdx)
{
    OverworldWildSlot *slot = &sWildSlots[slotIdx];
    u16 species = slot->species;
    u8 level = slot->level;
    u8 form = slot->form;
    BOOL isShiny = slot->isShiny;

    DespawnSlot(slotIdx);

    if (Party_GetCurrentCount(SaveData_GetParty(fieldSystem->saveData)) == 0) {
        return;
    }

    FieldBattleDTO *dto = FieldBattleDTO_New(HEAP_ID_FIELD2, BATTLE_TYPE_WILD_MON);
    FieldBattleDTO_Init(dto, fieldSystem);
    CreateWildMon_Scripted(fieldSystem, species, level, dto);

    Pokemon *wildMon = Party_GetPokemonBySlotIndex(dto->parties[BATTLER_ENEMY_1], 0);
    if (wildMon != NULL) {
        if (form > 0) {
            Pokemon_SetValue(wildMon, MON_DATA_FORM, &form);
        }
        if (isShiny) {
            u32 otId = TrainerInfo_ID(SaveData_GetTrainerInfo(fieldSystem->saveData));
            u32 pid = Pokemon_GetValue(wildMon, MON_DATA_PERSONALITY, NULL);
            u16 high = (u16)(pid >> 16);
            u16 idLow = (u16)(otId & 0xFFFF);
            u16 idHigh = (u16)(otId >> 16);
            u16 low = high ^ idLow ^ idHigh;
            u32 shinyPid = ((u32)high << 16) | low;
            Pokemon_SetValue(wildMon, MON_DATA_PERSONALITY, &shinyPid);
        }
    }

    Encounter_NewVsWild(fieldSystem, dto);
}

static void GetStepDeltaForDir(int dir, int *outDx, int *outDz)
{
    *outDx = 0;
    *outDz = 0;
    switch (dir) {
    case DIR_NORTH:
        *outDz = -1;
        break;
    case DIR_SOUTH:
        *outDz = 1;
        break;
    case DIR_WEST:
        *outDx = -1;
        break;
    case DIR_EAST:
        *outDx = 1;
        break;
    default:
        break;
    }
}

BOOL OverworldWild_TryTriggerTouchEncounter(FieldSystem *fieldSystem, u16 heldKeys, u8 playerDir)
{
    if (!SIM_Config_prj_GetConfig()->overworldWildPokemon || fieldSystem == NULL || fieldSystem->playerAvatar == NULL) {
        return FALSE;
    }

    int playerX = PlayerAvatar_GetXPos(fieldSystem->playerAvatar);
    int playerZ = PlayerAvatar_GetZPos(fieldSystem->playerAvatar);

    // Determine if the player is actively walking in a direction
    int stepDir = DIR_NONE;
    if (heldKeys & PAD_KEY_UP) {
        stepDir = DIR_NORTH;
    } else if (heldKeys & PAD_KEY_DOWN) {
        stepDir = DIR_SOUTH;
    } else if (heldKeys & PAD_KEY_LEFT) {
        stepDir = DIR_WEST;
    } else if (heldKeys & PAD_KEY_RIGHT) {
        stepDir = DIR_EAST;
    } else if (PlayerAvatar_GetMoveState(fieldSystem->playerAvatar) != AVATAR_MOVE_STATE_NONE) {
        stepDir = playerDir;
    }

    int stepDx = 0;
    int stepDz = 0;
    GetStepDeltaForDir(stepDir, &stepDx, &stepDz);
    int frontX = playerX + stepDx;
    int frontZ = playerZ + stepDz;

    for (int i = 0; i < OVERWORLD_WILD_MAX_ACTIVE; i++) {
        if (!sWildSlots[i].active || sWildSlots[i].mapObj == NULL) {
            continue;
        }

        int objX = MapObject_GetX(sWildSlots[i].mapObj);
        int objZ = MapObject_GetZ(sWildSlots[i].mapObj);

        // 1. Overlapping tile
        if (objX == playerX && objZ == playerZ) {
            TriggerSlotEncounter(fieldSystem, i);
            return TRUE;
        }

        // 2. Player walks directly into the wild Pokemon's tile
        if (stepDir != DIR_NONE && objX == frontX && objZ == frontZ) {
            TriggerSlotEncounter(fieldSystem, i);
            return TRUE;
        }

        // 3. Wild Pokemon is actively moving toward the player's adjacent tile
        int objMoveDir = MapObject_GetMovingDir(sWildSlots[i].mapObj);
        if (objMoveDir != DIR_NONE) {
            int odx = 0;
            int odz = 0;
            GetStepDeltaForDir(objMoveDir, &odx, &odz);
            if ((objX + odx) == playerX && (objZ + odz) == playerZ) {
                TriggerSlotEncounter(fieldSystem, i);
                return TRUE;
            }
        }
    }

    return FALSE;
}

BOOL OverworldWild_TryInteract(FieldSystem *fieldSystem, MapObject *targetObj)
{
    if (!SIM_Config_prj_GetConfig()->overworldWildPokemon || targetObj == NULL) {
        return FALSE;
    }

    for (int i = 0; i < OVERWORLD_WILD_MAX_ACTIVE; i++) {
        if (sWildSlots[i].active && sWildSlots[i].mapObj == targetObj) {
            TriggerSlotEncounter(fieldSystem, i);
            return TRUE;
        }
    }

    return FALSE;
}

void OverworldWild_Update(FieldSystem *fieldSystem)
{
    if (fieldSystem == NULL || fieldSystem->playerAvatar == NULL || fieldSystem->mapObjMan == NULL) {
        return;
    }

    int targetDensity = GetTargetDensity();
    if (targetDensity <= 0) {
        OverworldWild_ClearAll(fieldSystem);
        return;
    }

    // Refresh spawns when entering a new area/route
    if (fieldSystem->location->mapHeaderID != sLastMapHeaderID) {
        OverworldWild_ClearAll(fieldSystem);
        sLastMapHeaderID = fieldSystem->location->mapHeaderID;
        PopulateAreaBatch(fieldSystem);
        sNeedsAreaSpawn = FALSE;
        sRespawnCooldownTimer = OVERWORLD_WILD_RESPAWN_COOLDOWN;
        return;
    }

    if (sNeedsAreaSpawn) {
        PopulateAreaBatch(fieldSystem);
        sNeedsAreaSpawn = FALSE;
        sRespawnCooldownTimer = OVERWORLD_WILD_RESPAWN_COOLDOWN;
        return;
    }

    // Trim excess slots immediately if user lowered spawn density in Settings
    int activeCount = 0;
    for (int i = 0; i < OVERWORLD_WILD_MAX_ACTIVE; i++) {
        if (sWildSlots[i].active) {
            activeCount++;
            if (activeCount > targetDensity) {
                DespawnSlot(i);
                activeCount--;
            }
        }
    }

    int playerX = PlayerAvatar_GetXPos(fieldSystem->playerAvatar);
    int playerZ = PlayerAvatar_GetZPos(fieldSystem->playerAvatar);

    for (int i = 0; i < OVERWORLD_WILD_MAX_ACTIVE; i++) {
        if (!sWildSlots[i].active) {
            continue;
        }
        MapObject *obj = sWildSlots[i].mapObj;
        if (obj == NULL) {
            sWildSlots[i].active = FALSE;
            continue;
        }

        int dist = AbsDiff(MapObject_GetX(obj), playerX) + AbsDiff(MapObject_GetZ(obj), playerZ);
        if (dist > OVERWORLD_WILD_DESPAWN_DIST) {
            DespawnSlot(i);
            continue;
        }

        BOOL isWaterTile = FALSE;
        if (!IsValidWildEncounterTile(fieldSystem, MapObject_GetX(obj), MapObject_GetZ(obj), &isWaterTile) ||
            (isWaterTile != sWildSlots[i].isWaterSpawn)) {
            DespawnSlot(i);
        }
    }

    if (sRespawnCooldownTimer > 0) {
        sRespawnCooldownTimer--;
    } else {
        TrySpawnOneWildPokemon(fieldSystem, 12);
        sRespawnCooldownTimer = OVERWORLD_WILD_RESPAWN_COOLDOWN;
    }
}

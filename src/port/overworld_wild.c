#include "port/overworld_wild.h"
#include "port/following_hack.h"
#include "port/sim_config_prj.h"
#include "constants/map_object.h"
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
static u16 sSpawnCooldownTimer = 0;
static enum MapHeaderID sLastMapHeaderID = (enum MapHeaderID)-1;

static inline int AbsDiff(int a, int b)
{
    return (a > b) ? (a - b) : (b - a);
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

static void DespawnSlot(int idx)
{
    if (!sWildSlots[idx].active) {
        return;
    }
    if (sWildSlots[idx].mapObj != NULL) {
        MapObject_Delete(sWildSlots[idx].mapObj);
        sWildSlots[idx].mapObj = NULL;
    }
    sWildSlots[idx].active = FALSE;
}

void OverworldWild_ClearAll(FieldSystem *fieldSystem)
{
    (void)fieldSystem;
    for (int i = 0; i < OVERWORLD_WILD_MAX_ACTIVE; i++) {
        DespawnSlot(i);
    }
    sSpawnCooldownTimer = OVERWORLD_WILD_SPAWN_COOLDOWN;
}

void OverworldWild_Init(FieldSystem *fieldSystem)
{
    memset(sWildSlots, 0, sizeof(sWildSlots));
    sSpawnCooldownTimer = OVERWORLD_WILD_SPAWN_COOLDOWN;
    sLastMapHeaderID = fieldSystem ? fieldSystem->location->mapHeaderID : (enum MapHeaderID)-1;
}

static BOOL IsTileOccupiedByAnyObject(const MapObjectManager *mapObjMan, int x, int z)
{
    MapObject *obj = NULL;
    int index = 0;
    while (MapObjectMan_FindObjectWithStatus(mapObjMan, &obj, &index, MAP_OBJ_STATUS_0)) {
        if (MapObject_GetX(obj) == x && MapObject_GetZ(obj) == z) {
            return TRUE;
        }
        if (MapObject_GetXPrev(obj) == x && MapObject_GetZPrev(obj) == z) {
            return TRUE;
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

static void TrySpawnOneWildPokemon(FieldSystem *fieldSystem)
{
    if (!FollowingHack_IsActive()) {
        return;
    }
    if (!MapHeader_HasWildEncounters(fieldSystem->location->mapHeaderID)) {
        return;
    }

    int freeSlot = -1;
    for (int i = 0; i < OVERWORLD_WILD_MAX_ACTIVE; i++) {
        if (!sWildSlots[i].active) {
            freeSlot = i;
            break;
        }
    }
    if (freeSlot == -1) {
        return;
    }

    int playerX = PlayerAvatar_GetXPos(fieldSystem->playerAvatar);
    int playerZ = PlayerAvatar_GetZPos(fieldSystem->playerAvatar);

    for (int attempt = 0; attempt < 6; attempt++) {
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

        if (IsTileOccupiedByAnyObject(fieldSystem->mapObjMan, targetX, targetZ)) {
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
            return;
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
        return;
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

    Pokemon *wildMon = Party_GetPokemonBySlotIndex(dto->parties[BATTLE_PARTY_ENEMY], 0);
    if (wildMon != NULL) {
        if (form > 0) {
            Pokemon_SetValue(wildMon, MON_DATA_FORM, &form);
        }
        if (isShiny) {
            u32 otId = TrainerInfo_ID(SaveData_GetTrainerInfo(fieldSystem->saveData));
            u32 pid = Pokemon_GetValue(wildMon, MON_DATA_PERSONALITY, NULL);
            // Force low 16 bits to match high 16 bits XOR otId for guaranteed shiny PID
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

BOOL OverworldWild_TryTriggerTouchEncounter(FieldSystem *fieldSystem)
{
    if (!SIM_Config_prj_GetConfig()->overworldWildPokemon || fieldSystem == NULL || fieldSystem->playerAvatar == NULL) {
        return FALSE;
    }

    int playerX = PlayerAvatar_GetXPos(fieldSystem->playerAvatar);
    int playerZ = PlayerAvatar_GetZPos(fieldSystem->playerAvatar);

    for (int i = 0; i < OVERWORLD_WILD_MAX_ACTIVE; i++) {
        if (!sWildSlots[i].active || sWildSlots[i].mapObj == NULL) {
            continue;
        }

        int objX = MapObject_GetX(sWildSlots[i].mapObj);
        int objZ = MapObject_GetZ(sWildSlots[i].mapObj);

        if (objX == playerX && objZ == playerZ) {
            TriggerSlotEncounter(fieldSystem, i);
            return TRUE;
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

    if (!SIM_Config_prj_GetConfig()->overworldWildPokemon) {
        OverworldWild_ClearAll(fieldSystem);
        return;
    }

    if (fieldSystem->location->mapHeaderID != sLastMapHeaderID) {
        OverworldWild_ClearAll(fieldSystem);
        sLastMapHeaderID = fieldSystem->location->mapHeaderID;
        return;
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

    if (sSpawnCooldownTimer > 0) {
        sSpawnCooldownTimer--;
    } else {
        TrySpawnOneWildPokemon(fieldSystem);
        sSpawnCooldownTimer = OVERWORLD_WILD_SPAWN_COOLDOWN;
    }
}

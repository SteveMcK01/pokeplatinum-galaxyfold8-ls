#ifndef POKEPLATINUM_OVERWORLD_WILD_H
#define POKEPLATINUM_OVERWORLD_WILD_H

#include <nitro.h>

#include "field/field_system_decl.h"
#include "map_object.h"

#define OVERWORLD_WILD_MAX_ACTIVE       4
#define OVERWORLD_WILD_BASE_LOCAL_ID    0xE0
#define OVERWORLD_WILD_SPAWN_MIN_DIST   3
#define OVERWORLD_WILD_SPAWN_MAX_DIST   8
#define OVERWORLD_WILD_DESPAWN_DIST     13
#define OVERWORLD_WILD_SPAWN_COOLDOWN   45

void OverworldWild_Init(FieldSystem *fieldSystem);
void OverworldWild_ClearAll(FieldSystem *fieldSystem);
void OverworldWild_Update(FieldSystem *fieldSystem);

BOOL OverworldWild_TryTriggerTouchEncounter(FieldSystem *fieldSystem);
BOOL OverworldWild_TryInteract(FieldSystem *fieldSystem, MapObject *targetObj);
BOOL OverworldWild_IsTransientWildObject(const MapObject *mapObj);

#endif // POKEPLATINUM_OVERWORLD_WILD_H

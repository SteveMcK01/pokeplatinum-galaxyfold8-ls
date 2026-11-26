#ifndef POKEPLATINUM_HONEY_TREE_H
#define POKEPLATINUM_HONEY_TREE_H

#include "constants/honey_tree.h"

#include "field/field_system.h"
#include "field/field_system_decl.h"
#include "generated/map_headers.h"
#include "overlay005/map_prop.h"

HoneyTreeShakeList *HoneyTree_ShakeDataInit(void);
void HoneyTree_FreeShakeData(HoneyTreeShakeList **param0);
BOOL HoneyTree_TryInteract(FieldSystem *fieldSystem, int *eventId);
u16 HoneyTree_GetTreeSlatherStatus(FieldSystem *fieldSystem);
void HoneyTree_SlatherTree(FieldSystem *fieldSystem);
void HoneyTree_StopShaking(FieldSystem *fieldSystem);
int HoneyTree_GetSpecies(FieldSystem *fieldSystem);
void ov5_021F0030(void *param0, const int param1, MapPropManager *const mapPropManager);
void HoneyTree_Unslather(FieldSystem *fieldSystem);
int HoneyTree_GetTreeIDForMap(enum MapHeaderID mapHeaderID);
enum MapHeaderID HoneyTree_GetMapForTreeID(int treeId);
BOOL HoneyTree_IsMunchlaxTree(u32 trainerId, u8 treeId);

#endif // POKEPLATINUM_HONEY_TREE_H

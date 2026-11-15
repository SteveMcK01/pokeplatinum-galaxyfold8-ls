#ifndef POKEPLATINUM_PORT_CATCH_EM_ALL_H
#define POKEPLATINUM_PORT_CATCH_EM_ALL_H

#include <nitro/types.h>
#include "overlay006/wild_encounters.h"

#ifdef __cplusplus
extern "C" {
#endif

BOOL CatchEmAll_IsEnabled(void);
void CatchEmAll_ApplyGrassSlots(int mapHeaderID, EncounterSlot *slots);
void CatchEmAll_ApplySuperRodSlots(int mapHeaderID, EncounterSlot *slots);

#ifdef __cplusplus
}
#endif

#endif

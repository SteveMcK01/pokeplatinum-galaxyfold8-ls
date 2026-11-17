#ifndef POKEPLATINUM_PORT_CATCH_EM_ALL_H
#define POKEPLATINUM_PORT_CATCH_EM_ALL_H

#include <nitro/types.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct CatchEmAllSlot {
    int species;
    u16 maxLevel;
    u16 minLevel;
} CatchEmAllSlot;

BOOL CatchEmAll_IsEnabled(void);
void CatchEmAll_ApplyGrassSlots(int mapHeaderID, CatchEmAllSlot *slots);
void CatchEmAll_ApplySurfSlots(int mapHeaderID, CatchEmAllSlot *slots);
void CatchEmAll_ApplySuperRodSlots(int mapHeaderID, CatchEmAllSlot *slots);

#ifdef __cplusplus
}
#endif

#endif

#ifndef POKEPLATINUM_POKETCH_AREA_RADAR_GRAPHICS_H
#define POKEPLATINUM_POKETCH_AREA_RADAR_GRAPHICS_H

#include "applications/poketch/poketch_task.h"
#include "bg_window.h"
#include "port/gear_data.h"
#include "string_gf.h"

#define AREA_RADAR_TASK_SLOTS 4

enum AreaRadarGraphicsTask {
    AREA_RADAR_GRAPHICS_INIT = 0,
    AREA_RADAR_GRAPHICS_REFRESH,
    AREA_RADAR_GRAPHICS_FREE,
};

typedef struct AreaRadarViewData {
    GearArea area;
    u8 filterMethod; // 0xFF = All methods, otherwise GearEncounterMethod
    u8 page;
    u8 totalPages;
} AreaRadarViewData;

typedef struct AreaRadarGraphics {
    const AreaRadarViewData *viewData;
    BgConfig *bgConfig;
    Window window;
    String *string;
    u32 activeTasks[POKETCH_TASK_SLOT_BASE + AREA_RADAR_TASK_SLOTS];
} AreaRadarGraphics;

BOOL PoketchAreaRadarGraphics_New(AreaRadarGraphics **dest, const AreaRadarViewData *viewData, BgConfig *bgConfig);
void PoketchAreaRadarGraphics_Free(AreaRadarGraphics *graphics);
void PoketchAreaRadarGraphics_StartTask(AreaRadarGraphics *graphics, enum AreaRadarGraphicsTask taskID);
BOOL PoketchAreaRadarGraphics_TaskIsNotActive(AreaRadarGraphics *graphics, enum AreaRadarGraphicsTask taskID);
BOOL PoketchAreaRadarGraphics_NoActiveTasks(AreaRadarGraphics *graphics);

#endif // POKEPLATINUM_POKETCH_AREA_RADAR_GRAPHICS_H

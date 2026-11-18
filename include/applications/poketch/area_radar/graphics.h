#ifndef POKEPLATINUM_POKETCH_AREA_RADAR_GRAPHICS_H
#define POKEPLATINUM_POKETCH_AREA_RADAR_GRAPHICS_H

#include "applications/poketch/poketch_task.h"
#include "bg_window.h"
#include "string_gf.h"

#define AREA_RADAR_TASK_SLOTS  4
#define AREA_RADAR_MAX_ENTRIES 32

enum AreaRadarGraphicsTask {
    AREA_RADAR_GRAPHICS_INIT = 0,
    AREA_RADAR_GRAPHICS_REFRESH,
    AREA_RADAR_GRAPHICS_FREE,
};

enum AreaRadarIconFlag {
    AREA_RADAR_ICON_GRASS = (1 << 0),
    AREA_RADAR_ICON_DAY   = (1 << 1),
    AREA_RADAR_ICON_NIGHT = (1 << 2),
    AREA_RADAR_ICON_SURF  = (1 << 3),
    AREA_RADAR_ICON_ROD   = (1 << 4),
    AREA_RADAR_ICON_RADAR = (1 << 5),
    AREA_RADAR_ICON_SWARM = (1 << 6),
    AREA_RADAR_ICON_GBA   = (1 << 7),
};

typedef struct AreaRadarEntry {
    u16 species;
    u16 iconMask;
    u8 minLevel;
    u8 maxLevel;
    u8 caught;
} AreaRadarEntry;

typedef struct AreaRadarViewData {
    BOOL valid;
    int mapHeaderID;
    char name[32];
    int timeOfDay;
    u16 radarChain;
    u8 entryCount;
    u8 caughtCount;
    u8 page;
    u8 totalPages;
    AreaRadarEntry entries[AREA_RADAR_MAX_ENTRIES];
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

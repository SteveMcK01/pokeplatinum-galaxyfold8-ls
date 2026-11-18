#include <nitro.h>
#include <nitro/sinit.h>
#include <string.h>

#include "applications/poketch/area_radar/graphics.h"
#include "applications/poketch/poketch_system.h"

#include "constants/species.h"
#include "constants/wild_encounters.h"

#include "bg_window.h"
#include "field/field_system.h"
#include "heap.h"
#include "map_header.h"
#include "map_header_data.h"
#include "port/debug_field.h"
#include "port/gear_data.h"
#include "sys_task.h"
#include "sys_task_manager.h"
#include "touch_screen.h"

#define POLL_INTERVAL_FRAMES 15
#define ROWS_PER_PAGE        7

typedef struct PoketchAreaRadar {
    u8 state;
    u8 subState;
    u8 shouldExit;
    u8 pollTimer;
    AreaRadarViewData viewData;
    AreaRadarGraphics *graphics;
    PoketchSystem *poketchSys;
} PoketchAreaRadar;

typedef BOOL (*StateFunc)(PoketchAreaRadar *);

enum AreaRadarState {
    STATE_LOAD_APP = 0,
    STATE_UPDATE_LOOP,
    STATE_SHUTDOWN,
};

static void NitroStaticInit(void);
static BOOL New(void **appData, PoketchSystem *poketchSys, BgConfig *bgConfig, u32 appID);
static BOOL Init(PoketchAreaRadar *appData, PoketchSystem *poketchSys, BgConfig *bgConfig, u32 appID);
static void Free(PoketchAreaRadar *appData);
static void Exit(void *appData);
static void Task_Main(SysTask *task, void *appData);
static void ChangeState(PoketchAreaRadar *appData, enum AreaRadarState newState);
static BOOL State_LoadApp(PoketchAreaRadar *appData);
static BOOL State_UpdateApp(PoketchAreaRadar *appData);
static BOOL State_UnloadApp(PoketchAreaRadar *appData);

static void NitroStaticInit(void)
{
    PoketchSystem_SetAppFunctions(New, Exit);
}

#ifdef SDK_PORT
void PoketchAppStaticInit_AreaRadar(void)
{
    NitroStaticInit();
}
#endif

static u16 MapMethodToIcon(u8 method)
{
    switch (method) {
    case GEAR_ENC_GRASS:
        return AREA_RADAR_ICON_GRASS;
    case GEAR_ENC_SURF:
        return AREA_RADAR_ICON_SURF;
    case GEAR_ENC_OLD_ROD:
    case GEAR_ENC_GOOD_ROD:
    case GEAR_ENC_SUPER_ROD:
        return AREA_RADAR_ICON_ROD;
    case GEAR_ENC_RADAR:
        return AREA_RADAR_ICON_RADAR;
    case GEAR_ENC_SWARM:
        return AREA_RADAR_ICON_SWARM;
    case GEAR_ENC_GBA:
        return AREA_RADAR_ICON_GBA;
    default:
        return AREA_RADAR_ICON_GRASS;
    }
}

static void AddOrMergeEntry(AreaRadarViewData *view, u16 species, u16 iconMask, u8 minLevel, u8 maxLevel)
{
    if (species == SPECIES_NONE || species == 0) {
        return;
    }

    for (int i = 0; i < view->entryCount; i++) {
        AreaRadarEntry *entry = &view->entries[i];
        if (entry->species == species) {
            entry->iconMask |= iconMask;
            if (minLevel > 0 && (entry->minLevel == 0 || minLevel < entry->minLevel)) {
                entry->minLevel = minLevel;
            }
            if (maxLevel > entry->maxLevel) {
                entry->maxLevel = maxLevel;
            }
            return;
        }
    }

    if (view->entryCount < AREA_RADAR_MAX_ENTRIES) {
        AreaRadarEntry *entry = &view->entries[view->entryCount++];
        entry->species = species;
        entry->iconMask = iconMask;
        entry->minLevel = minLevel;
        entry->maxLevel = maxLevel;
        entry->caught = Gear_SpeciesCaught(species) ? 1 : 0;
        if (entry->caught) {
            view->caughtCount++;
        }
    }
}

static void BuildViewData(AreaRadarViewData *view)
{
    u8 prevPage = view->page;
    int prevMapID = view->mapHeaderID;

    GearArea rawArea;
    Gear_GetArea(&rawArea);

    memset(view, 0, sizeof(*view));
    view->valid = rawArea.valid;
    view->mapHeaderID = rawArea.mapHeaderID;
    snprintf(view->name, sizeof(view->name), "%s", rawArea.name);
    view->timeOfDay = rawArea.timeOfDay;
    view->radarChain = rawArea.radarChain;

    // Collect all grass encounters across all times of day (General, Morning/Day, Night)
    FieldSystem *fieldSystem = DEBUG_GetFieldSystem();
    if (view->valid && fieldSystem != NULL && MapHeader_HasWildEncounters(view->mapHeaderID)) {
        const WildEncounters *data = MapHeaderData_GetWildEncounters(fieldSystem);
        if (data != NULL && data->grassEncounters.encounterRate != 0) {
            for (int i = 0; i < MAX_GRASS_ENCOUNTERS; i++) {
                if (i == 2 || i == 3) {
                    continue;
                }
                const GrassEncounter *enc = &data->grassEncounters.encounters[i];
                AddOrMergeEntry(view, (u16)enc->species, AREA_RADAR_ICON_GRASS, enc->level, enc->level);
            }
            for (int k = 0; k < 2; k++) {
                u8 lvl = data->grassEncounters.encounters[2 + k].level;
                AddOrMergeEntry(view, (u16)data->grassEncounters.encounters[2 + k].species, AREA_RADAR_ICON_DAY, lvl, lvl);
                AddOrMergeEntry(view, (u16)data->dayEncounters[k], AREA_RADAR_ICON_DAY, lvl, lvl);
                AddOrMergeEntry(view, (u16)data->nightEncounters[k], AREA_RADAR_ICON_NIGHT, lvl, lvl);
            }
        }
    }

    // Merge live Gear_GetArea entries (CatchEmAll, Surf, Rods, Radar, Swarm, Dual-Slot, Daily)
    for (int i = 0; i < rawArea.count; i++) {
        const GearEncounter *enc = &rawArea.entries[i];
        AddOrMergeEntry(view, enc->species, MapMethodToIcon(enc->method), enc->minLevel, enc->maxLevel);
    }

    // If a species is available in all-day grass, clear redundant time-of-day flags
    for (int i = 0; i < view->entryCount; i++) {
        if (view->entries[i].iconMask & AREA_RADAR_ICON_GRASS) {
            view->entries[i].iconMask &= ~(AREA_RADAR_ICON_DAY | AREA_RADAR_ICON_NIGHT);
        }
    }

    int pages = (view->entryCount + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE;
    if (pages < 1) {
        pages = 1;
    }
    view->totalPages = (u8)pages;
    if (view->mapHeaderID == prevMapID && prevPage < view->totalPages) {
        view->page = prevPage;
    } else {
        view->page = 0;
    }
}

static void AdvancePage(AreaRadarViewData *view)
{
    if (view->totalPages > 1) {
        view->page = (u8)((view->page + 1) % view->totalPages);
    }
}

static BOOL New(void **appData, PoketchSystem *poketchSys, BgConfig *bgConfig, u32 appID)
{
    PoketchAreaRadar *radar = Heap_Alloc(HEAP_ID_POKETCH_APP, sizeof(PoketchAreaRadar));

    if (radar != NULL) {
        if (Init(radar, poketchSys, bgConfig, appID)) {
            if (SysTask_Start(Task_Main, radar, 1) != NULL) {
                *appData = radar;
                return TRUE;
            }
        }
        Heap_Free(radar);
    }

    return FALSE;
}

static BOOL Init(PoketchAreaRadar *appData, PoketchSystem *poketchSys, BgConfig *bgConfig, u32 appID)
{
    memset(&appData->viewData, 0, sizeof(appData->viewData));
    BuildViewData(&appData->viewData);

    if (PoketchAreaRadarGraphics_New(&appData->graphics, &appData->viewData, bgConfig)) {
        appData->state = STATE_LOAD_APP;
        appData->subState = 0;
        appData->shouldExit = FALSE;
        appData->pollTimer = 0;
        appData->poketchSys = poketchSys;
        return TRUE;
    }

    return FALSE;
}

static void Free(PoketchAreaRadar *appData)
{
    PoketchAreaRadarGraphics_Free(appData->graphics);
    Heap_Free(appData);
}

static void Task_Main(SysTask *task, void *appData)
{
    static const StateFunc stateFuncs[] = {
        State_LoadApp,
        State_UpdateApp,
        State_UnloadApp,
    };

    PoketchAreaRadar *radar = appData;

    if (radar->state < NELEMS(stateFuncs)) {
        if (stateFuncs[radar->state](radar)) {
            Free(radar);
            SysTask_Done(task);
            PoketchSystem_NotifyAppUnloaded(radar->poketchSys);
        }
    }
}

static void Exit(void *appData)
{
    ((PoketchAreaRadar *)appData)->shouldExit = TRUE;
}

static void ChangeState(PoketchAreaRadar *appData, enum AreaRadarState newState)
{
    if (!appData->shouldExit) {
        appData->state = newState;
    } else {
        appData->state = STATE_SHUTDOWN;
    }
    appData->subState = 0;
}

static BOOL State_LoadApp(PoketchAreaRadar *appData)
{
    switch (appData->subState) {
    case 0:
        PoketchAreaRadarGraphics_StartTask(appData->graphics, AREA_RADAR_GRAPHICS_INIT);
        appData->subState++;
        break;
    case 1:
        if (PoketchAreaRadarGraphics_TaskIsNotActive(appData->graphics, AREA_RADAR_GRAPHICS_INIT)) {
            PoketchSystem_NotifyAppLoaded(appData->poketchSys);
            ChangeState(appData, STATE_UPDATE_LOOP);
        }
        break;
    }

    return FALSE;
}

static BOOL State_UpdateApp(PoketchAreaRadar *appData)
{
    if (appData->shouldExit) {
        ChangeState(appData, STATE_SHUTDOWN);
        return FALSE;
    }

    if (!PoketchAreaRadarGraphics_TaskIsNotActive(appData->graphics, AREA_RADAR_GRAPHICS_REFRESH)) {
        return FALSE;
    }

    // Touch screen tap advances page when a route has > 7 species
    if (!PoketechSystem_IsRunningTask(appData->poketchSys)) {
        u32 x, y;
        if (TouchScreen_GetTapState(&x, &y) && INSIDE_POKETCH_SCREEN(x, y)) {
            if (appData->viewData.totalPages > 1) {
                AdvancePage(&appData->viewData);
                PoketchAreaRadarGraphics_StartTask(appData->graphics, AREA_RADAR_GRAPHICS_REFRESH);
            }
            return FALSE;
        }
    }

    // Periodically poll live area encounter data and caught status
    if (++appData->pollTimer >= POLL_INTERVAL_FRAMES) {
        appData->pollTimer = 0;
        AreaRadarViewData latest;
        memset(&latest, 0, sizeof(latest));
        latest.mapHeaderID = appData->viewData.mapHeaderID;
        latest.page = appData->viewData.page;
        BuildViewData(&latest);

        if (memcmp(&latest, &appData->viewData, sizeof(AreaRadarViewData)) != 0) {
            appData->viewData = latest;
            PoketchAreaRadarGraphics_StartTask(appData->graphics, AREA_RADAR_GRAPHICS_REFRESH);
        }
    }

    return FALSE;
}

static BOOL State_UnloadApp(PoketchAreaRadar *appData)
{
    switch (appData->subState) {
    case 0:
        PoketchAreaRadarGraphics_StartTask(appData->graphics, AREA_RADAR_GRAPHICS_FREE);
        appData->subState++;
        break;
    case 1:
        if (PoketchAreaRadarGraphics_NoActiveTasks(appData->graphics)) {
            return TRUE;
        }
        break;
    }

    return FALSE;
}

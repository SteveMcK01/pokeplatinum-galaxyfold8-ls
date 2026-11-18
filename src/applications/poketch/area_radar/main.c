#include <nitro.h>
#include <nitro/sinit.h>
#include <string.h>

#include "applications/poketch/area_radar/graphics.h"
#include "applications/poketch/poketch_system.h"

#include "bg_window.h"
#include "heap.h"
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

static int CountMatches(const GearArea *area, u8 filterMethod)
{
    if (!area->valid || area->count == 0) {
        return 0;
    }
    if (filterMethod == 0xFF) {
        return area->count;
    }
    int count = 0;
    for (int i = 0; i < area->count; i++) {
        if (area->entries[i].method == filterMethod) {
            count++;
        }
    }
    return count;
}

static void RecalcPaging(AreaRadarViewData *view)
{
    int matches = CountMatches(&view->area, view->filterMethod);
    if (matches == 0 && view->filterMethod != 0xFF) {
        view->filterMethod = 0xFF;
        matches = CountMatches(&view->area, view->filterMethod);
    }
    int pages = (matches + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE;
    if (pages < 1) {
        pages = 1;
    }
    view->totalPages = (u8)pages;
    if (view->page >= view->totalPages) {
        view->page = 0;
    }
}

static void AdvanceFilterOrPage(AreaRadarViewData *view)
{
    if (!view->area.valid || view->area.count == 0) {
        return;
    }

    // Advance within current filter first if multi-page
    if (view->page + 1 < view->totalPages) {
        view->page++;
        return;
    }

    // Otherwise cycle through available methods: ALL -> method0 -> method1 -> ... -> ALL
    view->page = 0;
    if (view->filterMethod == 0xFF) {
        view->filterMethod = view->area.entries[0].method;
    } else {
        BOOL foundCurrent = FALSE;
        u8 nextMethod = 0xFF;
        for (int i = 0; i < view->area.count; i++) {
            u8 m = view->area.entries[i].method;
            if (m == view->filterMethod) {
                foundCurrent = TRUE;
            } else if (foundCurrent) {
                nextMethod = m;
                break;
            }
        }
        view->filterMethod = nextMethod;
    }
    RecalcPaging(view);
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
    appData->viewData.filterMethod = 0xFF;
    appData->viewData.page = 0;
    Gear_GetArea(&appData->viewData.area);
    RecalcPaging(&appData->viewData);

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

    // Touch screen tap cycles page / encounter method filter
    if (!PoketechSystem_IsRunningTask(appData->poketchSys)) {
        u32 x, y;
        if (TouchScreen_GetTapState(&x, &y) && INSIDE_POKETCH_SCREEN(x, y)) {
            AdvanceFilterOrPage(&appData->viewData);
            PoketchAreaRadarGraphics_StartTask(appData->graphics, AREA_RADAR_GRAPHICS_REFRESH);
            return FALSE;
        }
    }

    // Periodically poll live area encounter data (map change, time of day, radar chain, swarms)
    if (++appData->pollTimer >= POLL_INTERVAL_FRAMES) {
        appData->pollTimer = 0;
        GearArea latest;
        Gear_GetArea(&latest);

        if (memcmp(&latest, &appData->viewData.area, sizeof(GearArea)) != 0) {
            BOOL mapChanged = (latest.mapHeaderID != appData->viewData.area.mapHeaderID);
            appData->viewData.area = latest;
            if (mapChanged) {
                appData->viewData.filterMethod = 0xFF;
                appData->viewData.page = 0;
            }
            RecalcPaging(&appData->viewData);
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

#include "applications/poketch/area_radar/graphics.h"

#include <nitro.h>
#include <stdio.h>
#include <string.h>

#include "applications/poketch/poketch_graphics.h"
#include "applications/poketch/poketch_system.h"
#include "applications/poketch/poketch_task.h"

#include "bg_window.h"
#include "font.h"
#include "heap.h"
#include "port/charcode_convert.h"
#include "port/gear_data.h"
#include "string_gf.h"
#include "sys_task_manager.h"
#include "text.h"

#include "res/sound/pl_sound_data.naix"

#define LCD_BG_COLOR      4
#define LCD_SHADOW_COLOR  8
#define LCD_FG_COLOR      1

#define COLOR_NORMAL      TEXT_COLOR(LCD_FG_COLOR, LCD_SHADOW_COLOR, LCD_BG_COLOR)
#define COLOR_HEADER      TEXT_COLOR(LCD_BG_COLOR, LCD_SHADOW_COLOR, LCD_FG_COLOR)

#define ROWS_PER_PAGE     7

static void EndTask(PoketchTaskManager *taskMan);
static void Task_DrawBackground(SysTask *task, void *taskMan);
static void Task_RefreshScreen(SysTask *task, void *taskMan);
static void Task_FreeBackground(SysTask *task, void *taskMan);
static void RenderAreaRadar(AreaRadarGraphics *graphics);

static const char *MethodShortName(u8 method)
{
    switch (method) {
    case 0xFF:
        return "ALL";
    case GEAR_ENC_GRASS:
        return "GRASS";
    case GEAR_ENC_SURF:
        return "SURF";
    case GEAR_ENC_OLD_ROD:
        return "OLD ROD";
    case GEAR_ENC_GOOD_ROD:
        return "GOOD ROD";
    case GEAR_ENC_SUPER_ROD:
        return "SUPER ROD";
    case GEAR_ENC_RADAR:
        return "RADAR";
    case GEAR_ENC_SWARM:
        return "SWARM";
    case GEAR_ENC_GBA:
        return "GBA SLOT";
    default:
        return "WILD";
    }
}

static const char *MethodRowTag(u8 method)
{
    switch (method) {
    case GEAR_ENC_GRASS:
        return "GRS";
    case GEAR_ENC_SURF:
        return "SRF";
    case GEAR_ENC_OLD_ROD:
        return "OLD";
    case GEAR_ENC_GOOD_ROD:
        return "GOD";
    case GEAR_ENC_SUPER_ROD:
        return "SUP";
    case GEAR_ENC_RADAR:
        return "RDR";
    case GEAR_ENC_SWARM:
        return "SWM";
    case GEAR_ENC_GBA:
        return "GBA";
    default:
        return "WLD";
    }
}

static const char *TimeShortName(int timeOfDay)
{
    switch (timeOfDay) {
    case 0:
        return "MORN";
    case 1:
        return "DAY";
    case 2:
        return "EVE";
    case 3:
        return "NITE";
    case 4:
        return "LATE";
    default:
        return "";
    }
}

static void DrawAscii(AreaRadarGraphics *graphics, const char *text, u32 x, u32 y, TextColor color)
{
    charcode_t buf[64];
    CharCode_FromAscii(text, buf, NELEMS(buf));
    String_CopyChars(graphics->string, buf);
    Text_AddPrinterWithParamsAndColor(&graphics->window, FONT_SYSTEM, graphics->string, x, y, TEXT_SPEED_NO_TRANSFER, color, NULL);
}

BOOL PoketchAreaRadarGraphics_New(AreaRadarGraphics **dest, const AreaRadarViewData *viewData, BgConfig *bgConfig)
{
    AreaRadarGraphics *graphics = Heap_Alloc(HEAP_ID_POKETCH_APP, sizeof(AreaRadarGraphics));

    if (graphics != NULL) {
        PoketchTask_InitActiveTaskList(graphics->activeTasks, AREA_RADAR_TASK_SLOTS);
        graphics->viewData = viewData;
        graphics->bgConfig = PoketchGraphics_GetBgConfig();
        graphics->string = String_Init(96, HEAP_ID_POKETCH_APP);

        if (graphics->bgConfig != NULL && graphics->string != NULL) {
            *dest = graphics;
            return TRUE;
        }

        if (graphics->string != NULL) {
            String_Free(graphics->string);
        }
        Heap_Free(graphics);
    }

    return FALSE;
}

void PoketchAreaRadarGraphics_Free(AreaRadarGraphics *graphics)
{
    if (graphics != NULL) {
        if (graphics->string != NULL) {
            String_Free(graphics->string);
        }
        Heap_Free(graphics);
    }
}

static const PoketchTask sAreaRadarTasks[] = {
    { AREA_RADAR_GRAPHICS_INIT, Task_DrawBackground, 0 },
    { AREA_RADAR_GRAPHICS_REFRESH, Task_RefreshScreen, 0 },
    { AREA_RADAR_GRAPHICS_FREE, Task_FreeBackground, 0 },
    { 0 }
};

void PoketchAreaRadarGraphics_StartTask(AreaRadarGraphics *graphics, enum AreaRadarGraphicsTask taskID)
{
    PoketchTask_Start(sAreaRadarTasks, taskID, graphics, graphics->viewData, graphics->activeTasks, 2, HEAP_ID_POKETCH_APP);
}

BOOL PoketchAreaRadarGraphics_TaskIsNotActive(AreaRadarGraphics *graphics, enum AreaRadarGraphicsTask taskID)
{
    return PoketchTask_TaskIsNotActive(graphics->activeTasks, taskID);
}

BOOL PoketchAreaRadarGraphics_NoActiveTasks(AreaRadarGraphics *graphics)
{
    return PoketchTask_NoActiveTasks(graphics->activeTasks);
}

static void EndTask(PoketchTaskManager *taskMan)
{
    AreaRadarGraphics *graphics = PoketchTask_GetTaskData(taskMan);
    PoketchTask_EndTask(graphics->activeTasks, taskMan);
}

static void Task_DrawBackground(SysTask *task, void *taskMan)
{
    static const BgTemplate bgTemplate = {
        .x = 0,
        .y = 0,
        .bufferSize = 0x800,
        .baseTile = 0,
        .screenSize = BG_SCREEN_SIZE_256x256,
        .colorMode = GX_BG_COLORMODE_16,
        .screenBase = GX_BG_SCRBASE_0x7000,
        .charBase = GX_BG_CHARBASE_0x00000,
        .bgExtPltt = GX_BG_EXTPLTT_01,
        .priority = 2,
        .areaOver = 0,
        .mosaic = FALSE,
    };
    AreaRadarGraphics *graphics = PoketchTask_GetTaskData(taskMan);

    Bg_InitFromTemplate(graphics->bgConfig, BG_LAYER_SUB_2, &bgTemplate, BG_TYPE_STATIC);
    Bg_ClearTilesRange(BG_LAYER_SUB_2, 32, 0, HEAP_ID_POKETCH_APP);
    Bg_ClearTilemap(graphics->bgConfig, BG_LAYER_SUB_2);
    PoketchGraphics_LoadActivePalette(0, 0);

    Window_Add(graphics->bgConfig, &graphics->window, BG_LAYER_SUB_2, 2, 2, 24, 20, 0, 1);
    Window_PutToTilemap(&graphics->window);

    RenderAreaRadar(graphics);
    Bg_CopyTilemapBufferToVRAM(graphics->bgConfig, BG_LAYER_SUB_2);

    GXSDispCnt dispCnt = GXS_GetDispCnt();
    GXS_SetVisiblePlane(dispCnt.visiblePlane | GX_PLANEMASK_BG2);

    EndTask(taskMan);
}

static void Task_RefreshScreen(SysTask *task, void *taskMan)
{
    AreaRadarGraphics *graphics = PoketchTask_GetTaskData(taskMan);

    RenderAreaRadar(graphics);
    Bg_CopyTilemapBufferToVRAM(graphics->bgConfig, BG_LAYER_SUB_2);
    PoketchSystem_PlaySoundEffect(SEQ_SE_DP_POKETCH_003_sseq);

    EndTask(taskMan);
}

static void Task_FreeBackground(SysTask *task, void *taskMan)
{
    AreaRadarGraphics *graphics = PoketchTask_GetTaskData(taskMan);

    Window_Remove(&graphics->window);
    Bg_FreeTilemapBuffer(graphics->bgConfig, BG_LAYER_SUB_2);

    EndTask(taskMan);
}

static void RenderAreaRadar(AreaRadarGraphics *graphics)
{
    const AreaRadarViewData *view = graphics->viewData;
    const GearArea *area = &view->area;
    char line[48];
    char badge[16];

    Window_FillTilemap(&graphics->window, LCD_BG_COLOR);

    // Top inverted LCD banner: Area Name + Chain / Page indicator
    Window_FillRectWithColor(&graphics->window, LCD_FG_COLOR, 0, 0, 192, 16);

    char areaTitle[19];
    if (area->valid && area->name[0] != '\0') {
        snprintf(areaTitle, sizeof(areaTitle), "%.18s", area->name);
    } else {
        snprintf(areaTitle, sizeof(areaTitle), "AREA RADAR");
    }
    DrawAscii(graphics, areaTitle, 3, 1, COLOR_HEADER);

    if (area->radarChain > 0) {
        snprintf(badge, sizeof(badge), "CH:%d", area->radarChain);
    } else if (view->totalPages > 1) {
        snprintf(badge, sizeof(badge), "%d/%d", view->page + 1, view->totalPages);
    } else {
        snprintf(badge, sizeof(badge), "%s", TimeShortName(area->timeOfDay));
    }
    u32 badgeW = (u32)(strlen(badge) * 6);
    u32 badgeX = badgeW < 188 ? (189 - badgeW) : 140;
    DrawAscii(graphics, badge, badgeX, 1, COLOR_HEADER);

    if (!area->valid || area->count == 0) {
        DrawAscii(graphics, "NO WILD POKEMON", 38, 64, COLOR_NORMAL);
        DrawAscii(graphics, "IN THIS AREA", 48, 80, COLOR_NORMAL);
        Window_LoadTiles(&graphics->window);
        return;
    }

    // Sub-header: Filter mode + Time of day + Tap hint
    snprintf(line, sizeof(line), "[%s] %s", MethodShortName(view->filterMethod), TimeShortName(area->timeOfDay));
    DrawAscii(graphics, line, 2, 18, COLOR_NORMAL);
    DrawAscii(graphics, "TAP:NEXT", 140, 18, COLOR_NORMAL);

    // Filter matching encounters and render active page
    int matchedIndices[GEAR_AREA_MAX_ENTRIES];
    int matchCount = 0;
    for (int i = 0; i < area->count; i++) {
        if (view->filterMethod == 0xFF || area->entries[i].method == view->filterMethod) {
            matchedIndices[matchCount++] = i;
        }
    }

    int start = view->page * ROWS_PER_PAGE;
    for (int row = 0; row < ROWS_PER_PAGE; row++) {
        int idxPos = start + row;
        if (idxPos >= matchCount) {
            break;
        }

        const GearEncounter *enc = &area->entries[matchedIndices[idxPos]];
        u32 y = 34 + (u32)(row * 18);

        char dexMark = '-';
        if (Gear_SpeciesCaught(enc->species)) {
            dexMark = '*';
        } else if (Gear_SpeciesSeen(enc->species)) {
            dexMark = 'o';
        }

        char speciesName[12];
        snprintf(speciesName, sizeof(speciesName), "%.10s", Gear_SpeciesName(enc->species));

        char lvlStr[10];
        if (enc->minLevel == enc->maxLevel) {
            snprintf(lvlStr, sizeof(lvlStr), "L%d", enc->minLevel);
        } else {
            snprintf(lvlStr, sizeof(lvlStr), "L%d-%d", enc->minLevel, enc->maxLevel);
        }

        char rightStr[10];
        if (enc->rate > 0) {
            if (view->filterMethod == 0xFF) {
                snprintf(rightStr, sizeof(rightStr), "%s %d%%", MethodRowTag(enc->method), enc->rate);
            } else {
                snprintf(rightStr, sizeof(rightStr), "%d%%", enc->rate);
            }
        } else {
            snprintf(rightStr, sizeof(rightStr), "%s", MethodRowTag(enc->method));
        }

        snprintf(line, sizeof(line), "%c%-10s %-5s", dexMark, speciesName, lvlStr);
        DrawAscii(graphics, line, 2, y, COLOR_NORMAL);

        u32 rightW = (u32)(strlen(rightStr) * 6);
        u32 rightX = rightW < 190 ? (190 - rightW) : 148;
        DrawAscii(graphics, rightStr, rightX, y, COLOR_NORMAL);
    }

    Window_LoadTiles(&graphics->window);
}

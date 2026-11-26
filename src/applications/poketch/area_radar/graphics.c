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
#include "../../../port/charcode_convert.h"
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
#define ROW_HEIGHT        20
#define LIST_START_Y      18

static void EndTask(PoketchTaskManager *taskMan);
static void Task_DrawBackground(SysTask *task, void *taskMan);
static void Task_RefreshScreen(SysTask *task, void *taskMan);
static void Task_FreeBackground(SysTask *task, void *taskMan);
static void RenderAreaRadar(AreaRadarGraphics *graphics);

// 12x12 Poketch LCD Icons ('#' = dark foreground, '.' = mid shadow, ' ' = transparent)
static const char *const sIconBallCaught[12] = {
    "    ####    ",
    "  ########  ",
    " ########## ",
    "############",
    "############",
    "####....####",
    "###..##..###",
    "###..##..###",
    "####....####",
    " ##      ## ",
    "  ##    ##  ",
    "    ####    ",
};

static const char *const sIconBallUncaught[12] = {
    "    ....    ",
    "  ..    ..  ",
    " .        . ",
    ".          .",
    ".          .",
    "....    ....",
    ".   ....   .",
    ".   ....   .",
    "....    ....",
    " .        . ",
    "  ..    ..  ",
    "    ....    ",
};

static const char *const sIconSun[12] = {
    "     #      ",
    "  #  #  #   ",
    "   . # .    ",
    "  . ### .   ",
    " ## ##### ##",
    "  . ### .   ",
    "   . # .    ",
    "  #  #  #   ",
    "     #      ",
    "            ",
    "            ",
    "            ",
};

static const char *const sIconMoon[12] = {
    "    ####    ",
    "   #####  # ",
    "  #####     ",
    " #####      ",
    " #####      ",
    " #####      ",
    " #####   #  ",
    "  #####     ",
    "   #####    ",
    "    ####    ",
    "            ",
    "            ",
};

static const char *const sIconGrass[12] = {
    "            ",
    "     #      ",
    " #   #   #  ",
    " #  ##   #  ",
    " ## ##  ##  ",
    " ## ### ##  ",
    " #########  ",
    " #########  ",
    "########### ",
    "            ",
    "            ",
    "            ",
};

static const char *const sIconSurf[12] = {
    "            ",
    " ##    ##   ",
    "#  #  #  #  ",
    "    ##    # ",
    "            ",
    " ##    ##   ",
    "#  #  #  #  ",
    "    ##    # ",
    "            ",
    " ##    ##   ",
    "#  #  #  #  ",
    "    ##    # ",
};

static const char *const sIconOldRod[12] = {
    "    ##      ",
    "   #  #     ",
    "  #   .     ",
    "  #   . ### ",
    " #     #   #",
    " #     #   #",
    "#      #   #",
    "##     #   #",
    "##      ### ",
    "            ",
    "            ",
    "            ",
};

static const char *const sIconGoodRod[12] = {
    "    ##      ",
    "   #  #     ",
    "  #   .     ",
    "  #   . ####",
    " #     #    ",
    " #     # ###",
    "#      #   #",
    "##     #   #",
    "##      ####",
    "            ",
    "            ",
    "            ",
};

static const char *const sIconSuperRod[12] = {
    "    ##      ",
    "   #  #     ",
    "  #   .     ",
    "  #   . ####",
    " #     #    ",
    " #      ### ",
    "#          #",
    "##         #",
    "##     #### ",
    "            ",
    "            ",
    "            ",
};

static const char *const sIconHoney[12] = {
    "   ######   ",
    "  #......#  ",
    "  ########  ",
    " #........# ",
    " #.######.# ",
    " #.#....#.# ",
    " #.######.# ",
    " #........# ",
    " #........# ",
    "  ########  ",
    "            ",
    "            ",
};

static const char *const sIconRadar[12] = {
    "     #      ",
    "  #  #  #   ",
    "   # . #    ",
    "    ###     ",
    "### #.# ### ",
    "    ###     ",
    "   # . #    ",
    "  #  #  #   ",
    "     #      ",
    "            ",
    "            ",
    "            ",
};

static const char *const sIconSwarm[12] = {
    "   ......   ",
    "  .      .  ",
    " .   ..   . ",
    ".    ..    .",
    ".  ......  .",
    ".    ..    .",
    ".    ..    .",
    " .        . ",
    "  .      .  ",
    "   ......   ",
    "            ",
    "            ",
};

static const char *const sIconSwarmAct[12] = {
    "   ######   ",
    "  ########  ",
    " ####  #### ",
    " ####  #### ",
    " ####  #### ",
    " ####  #### ",
    " ########## ",
    " ####  #### ",
    "  ########  ",
    "   ######   ",
    "            ",
    "            ",
};

static const char *const sIconGba[12] = {
    " ########## ",
    " #........# ",
    " #.######.# ",
    " #.#    #.# ",
    " #.#    #.# ",
    " #.######.# ",
    " #........# ",
    " #..####..# ",
    " ########## ",
    "            ",
    "            ",
    "            ",
};

static void DrawIcon12x12(Window *window, const char *const rows[12], u16 x, u16 y)
{
    for (u16 r = 0; r < 12; r++) {
        const char *row = rows[r];
        u16 c = 0;
        while (c < 12) {
            char ch = row[c];
            if (ch == ' ' || ch == '\0') {
                c++;
                continue;
            }
            u8 color = (ch == '#') ? LCD_FG_COLOR : LCD_SHADOW_COLOR;
            u16 span = 1;
            while ((c + span) < 12 && row[c + span] == ch) {
                span++;
            }
            Window_FillRectWithColor(window, color, (u16)(x + c), (u16)(y + r), span, 1);
            c = (u16)(c + span);
        }
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
    char badge[20];

    Window_FillTilemap(&graphics->window, LCD_BG_COLOR);

    // Top inverted LCD banner: Location Name + Caught/Total Route Counter
    Window_FillRectWithColor(&graphics->window, LCD_FG_COLOR, 0, 0, 192, 16);

    if (view->valid && view->entryCount > 0) {
        if (view->totalPages > 1) {
            snprintf(badge, sizeof(badge), "%d/%d P%d/%d", view->caughtCount, view->entryCount, view->page + 1, view->totalPages);
        } else {
            snprintf(badge, sizeof(badge), "%d/%d", view->caughtCount, view->entryCount);
        }
    } else {
        badge[0] = '\0';
    }

    u32 badgeW = (u32)(strlen(badge) * 6);
    u32 badgeX = badgeW < 188 ? (189 - badgeW) : 130;
    int maxTitleChars = (int)((badgeX > 8 ? (badgeX - 6) : 96) / 6);
    if (maxTitleChars > 22) {
        maxTitleChars = 22;
    }

    char areaTitle[24];
    if (view->valid && view->name[0] != '\0') {
        if (view->isMunchlaxTree) {
            const int titleChars = maxTitleChars > 4 ? (maxTitleChars - 4) : 1;
            snprintf(areaTitle, sizeof(areaTitle), "%.*s [M]", titleChars, view->name);
        } else {
            snprintf(areaTitle, sizeof(areaTitle), "%.*s", maxTitleChars, view->name);
        }
    } else {
        snprintf(areaTitle, sizeof(areaTitle), "AREA RADAR");
    }
    DrawAscii(graphics, areaTitle, 3, 1, COLOR_HEADER);

    if (badge[0] != '\0') {
        DrawAscii(graphics, badge, badgeX, 1, COLOR_HEADER);
    }

    if (!view->valid || view->entryCount == 0) {
        DrawAscii(graphics, "NO WILD POKEMON", 38, 64, COLOR_NORMAL);
        DrawAscii(graphics, "IN THIS AREA", 48, 80, COLOR_NORMAL);
        Window_LoadTiles(&graphics->window);
        return;
    }

    int start = view->page * ROWS_PER_PAGE;
    for (int row = 0; row < ROWS_PER_PAGE; row++) {
        int idx = start + row;
        if (idx >= view->entryCount) {
            break;
        }

        const AreaRadarEntry *entry = &view->entries[idx];
        u32 y = LIST_START_Y + (u32)(row * ROW_HEIGHT);

        // Subtle row separator line
        if (row > 0) {
            Window_FillRectWithColor(&graphics->window, LCD_SHADOW_COLOR, 2, (u16)(y - 1), 188, 1);
        }

        // Bold left-edge highlight bar on active Swarm rows
        if (entry->iconMask & AREA_RADAR_ICON_SWARM_ACT) {
            Window_FillRectWithColor(&graphics->window, LCD_FG_COLOR, 0, (u16)y, 2, ROW_HEIGHT - 2);
        }

        // 1. Pokeball caught status indicator
        DrawIcon12x12(&graphics->window, entry->caught ? sIconBallCaught : sIconBallUncaught, 2, (u16)(y + 3));

        // 2. Species name
        char speciesName[12];
        snprintf(speciesName, sizeof(speciesName), "%.9s", Gear_SpeciesName(entry->species));
        DrawAscii(graphics, speciesName, 16, y + 1, COLOR_NORMAL);

        // 3. Spawn rate percentage next to name
        if (entry->rate > 0) {
            char rateStr[8];
            snprintf(rateStr, sizeof(rateStr), "%d%%", entry->rate);
            DrawAscii(graphics, rateStr, 78, y + 1, COLOR_NORMAL);
        } else if (entry->iconMask & AREA_RADAR_ICON_SWARM) {
            DrawAscii(graphics, "OFF", 78, y + 1, COLOR_NORMAL);
        }

        // 4. Level range
        char lvlStr[10];
        if (entry->minLevel == entry->maxLevel) {
            snprintf(lvlStr, sizeof(lvlStr), "L%d", entry->minLevel);
        } else {
            snprintf(lvlStr, sizeof(lvlStr), "L%d-%d", entry->minLevel, entry->maxLevel);
        }
        DrawAscii(graphics, lvlStr, 105, y + 1, COLOR_NORMAL);

        // 5. Encounter method symbols (up to 4 icons on the right)
        const char *const *icons[12];
        int iconCount = 0;
        if (entry->iconMask & AREA_RADAR_ICON_SWARM_ACT) {
            icons[iconCount++] = sIconSwarmAct;
        }
        if (entry->iconMask & AREA_RADAR_ICON_GRASS) {
            icons[iconCount++] = sIconGrass;
        }
        if (entry->iconMask & AREA_RADAR_ICON_DAY) {
            icons[iconCount++] = sIconSun;
        }
        if (entry->iconMask & AREA_RADAR_ICON_NIGHT) {
            icons[iconCount++] = sIconMoon;
        }
        if (entry->iconMask & AREA_RADAR_ICON_SURF) {
            icons[iconCount++] = sIconSurf;
        }
        if (entry->iconMask & AREA_RADAR_ICON_OLD_ROD) {
            icons[iconCount++] = sIconOldRod;
        }
        if (entry->iconMask & AREA_RADAR_ICON_GOOD_ROD) {
            icons[iconCount++] = sIconGoodRod;
        }
        if (entry->iconMask & AREA_RADAR_ICON_SUPER_ROD) {
            icons[iconCount++] = sIconSuperRod;
        }
        if (entry->iconMask & AREA_RADAR_ICON_HONEY) {
            icons[iconCount++] = sIconHoney;
        }
        if (entry->iconMask & AREA_RADAR_ICON_RADAR) {
            icons[iconCount++] = sIconRadar;
        }
        if (entry->iconMask & AREA_RADAR_ICON_SWARM) {
            icons[iconCount++] = sIconSwarm;
        }
        if (entry->iconMask & AREA_RADAR_ICON_GBA) {
            icons[iconCount++] = sIconGba;
        }

        if (iconCount > 4) {
            iconCount = 4;
        }
        u16 iconX = (u16)(190 - (iconCount * 13));
        for (int k = 0; k < iconCount; k++) {
            DrawIcon12x12(&graphics->window, icons[k], (u16)(iconX + k * 13), (u16)(y + 3));
        }
    }

    Window_LoadTiles(&graphics->window);
}

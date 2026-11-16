#include <nitro.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <simulator/config/ini.h>

#include "port/sim_config_prj.h"

#include <simulator/sim.h>

#include <simulator/sim_carts.h>
// Shared by every game (libntr sim_carts.h)
#define SIM_CONFIG_PRJ_FILE_PATH SIM_SettingsPath("sim_config_prj.ini")

static int IniHandler(void* user, const char* section, const char* name,
                   const char* value);
static const char * BoolToString(BOOL arg);
static BOOL StringToBool(const char * str);

static SIM_Config_prj_type s_SIM_config_prj;

static int IniHandler(void* user, const char* section, const char* name,
                   const char* value)
{
    #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0

    SIM_Config_prj_type * config = (SIM_Config_prj_type *)user;

    // General section
    if(MATCH("General", "60Fps")) {
        config->enable60fps = StringToBool(value);
    }
    if(MATCH("General", "60FpsSpeedFix")) {
        config->enable60fpsSpeedFix = StringToBool(value);
    }
    if(MATCH("General", "InstantText")) {
        config->instantText = StringToBool(value);
    }
    if(MATCH("General", "InstantBars")) {
        config->instantBars = StringToBool(value);
    }
    if(MATCH("General", "HmFreeFieldMoves")) {
        config->hmFree = StringToBool(value);
    }
    if(MATCH("General", "HmNoBadges")) {
        config->hmNoBadges = StringToBool(value);
    }
    if(MATCH("General", "FollowerPokemon")) {
        config->followerPokemon = StringToBool(value);
    }
    if(MATCH("General", "Gen5Screens")) {
        config->gen5Screens = StringToBool(value);
    }
    if(MATCH("General", "Gen5Battles")) {
        config->gen5Battles = StringToBool(value);
    }
    if(MATCH("General", "RepelPrompt")) {
        config->repelPrompt = StringToBool(value);
    }
    if(MATCH("General", "ExpShareAll")) {
        config->expShareAll = StringToBool(value);
    }
    if(MATCH("General", "HardLevelCap")) {
        config->hardLevelCap = StringToBool(value);
    }
    if(MATCH("General", "ClockOffsetMinutes")) {
        config->clockOffsetMinutes = atoi(value);
    }
    if(MATCH("Nuzlocke", "Enabled")) {
        config->nuzlocke = StringToBool(value);
    }
    if(MATCH("Nuzlocke", "DupesClause")) {
        config->nuzlockeDupes = StringToBool(value);
    }
    if(MATCH("Nuzlocke", "ShinyClause")) {
        config->nuzlockeShiny = StringToBool(value);
    }
    if(MATCH("Nuzlocke", "FaintedToPC")) {
        config->nuzlockeToPC = StringToBool(value);
    }
    if(MATCH("General", "DrawEveryFrame")) {
        config->drawEveryFrame = StringToBool(value);
    }
    if(MATCH("General", "60FpsEverywhere")) {
        config->enable60fpsEverywhere = StringToBool(value);
    }
    if(MATCH("General", "EnableAsserts")) {
        config->enableAsserts = StringToBool(value);
    }

    // Cheats section
    if(MATCH("Cheats", "WalkThroughWalls")) {
        config->walkThroughWalls = StringToBool(value);
    }

    if(MATCH("Cheats", "DisableRandomEncounters")) {
        config->disableRandomEncounters = StringToBool(value);
    }

    if(MATCH("Cheats", "RunFromTrainerBattles")) {
        config->runFromTrainerBattles = StringToBool(value);
    }
    if(MATCH("Cheats", "ShinyOdds")) {
        config->shinyOdds = atoi(value);
    }
    if(MATCH("Cheats", "ExpPercent")) {
        config->expPercent = atoi(value);
    }
    if(MATCH("Cheats", "AlwaysCatch")) {
        config->alwaysCatch = StringToBool(value);
    }
    if(MATCH("Cheats", "InfinitePP")) {
        config->infinitePP = StringToBool(value);
    }
    if(MATCH("Cheats", "AlwaysEscape")) {
        config->alwaysEscape = StringToBool(value);
    }
    if(MATCH("Cheats", "FastEggs")) {
        config->fastEggs = StringToBool(value);
    }
    if(MATCH("Cheats", "AnyTM")) {
        config->anyTM = StringToBool(value);
    }
    if(MATCH("Cheats", "CatchEmAll")) {
        config->catchEmAll = StringToBool(value);
    }
    if(MATCH("Cheats", "TradeEvos")) {
        config->tradeEvos = StringToBool(value);
    }
    if(MATCH("Cheats", "ReusableTMs")) {
        config->reusableTMs = StringToBool(value);
    }
    if(MATCH("Cheats", "InfiniteTMs")) {
        config->infiniteTMs = StringToBool(value);
    }
    if(MATCH("Features", "OverworldWildPokemon") || MATCH("Cheats", "OverworldWildPokemon")) {
        config->overworldWildPokemon = StringToBool(value);
    }
    if(MATCH("Features", "OverworldWildDensity") || MATCH("Cheats", "OverworldWildDensity")) {
        int d = atoi(value);
        config->overworldWildDensity = (d >= 1 && d <= 6) ? d : 3;
    }

    // Saves section
    if(MATCH("Saves", "Slot")) {
        config->saveSlot = atoi(value);
    }
    if(MATCH("Saves", "Autosave")) {
        config->autosave = StringToBool(value);
    }
    if(MATCH("Saves", "AutosaveMinutes")) {
        config->autosaveMinutes = atoi(value);
    }
    if(MATCH("Saves", "AutosaveOnExit")) {
        config->autosaveOnExit = StringToBool(value);
    }

    // Gear section
    if(MATCH("Gear", "Enabled")) {
        config->gearEnabled = StringToBool(value);
    }
    if(MATCH("Gear", "BattleControls")) {
        config->gearBattleControls = StringToBool(value);
    }
    if(MATCH("Gear", "Discovery")) {
        config->gearDiscovery = atoi(value);
    }

    // GBA slot section
    if(MATCH("GBA", "Game")) {
        config->gbaSlotGame = atoi(value);
    }
    if(MATCH("TouchOverlay", "Enabled")) {
        config->touchOverlay = StringToBool(value);
    }
    if(MATCH("TouchOverlay", "Opacity")) {
        config->touchOverlayOpacity = (float)atof(value);
    }
    if(MATCH("TouchOverlay", "Scale")) {
        config->touchOverlayScale = (float)atof(value);
    }
    if(MATCH("TouchOverlay", "TopRightOverlay")) {
        config->topRightOverlay = StringToBool(value);
    }
    if(MATCH("TouchOverlay", "TopRightOverlayScale")) {
        config->topRightOverlayScale = (float)atof(value);
    }
    if(MATCH("TouchOverlay", "FullScreenCompanion")) {
        config->fullScreenCompanion = StringToBool(value);
    }
    if(MATCH("TouchOverlay", "TopRightOverlayMinimized")) {
        config->topRightOverlayMinimized = StringToBool(value);
    }

    return 1;
}

static const char * BoolToString(BOOL arg)
{
    const char * ret = "false";
    if(arg) {
        ret = "true";
    }

    return ret;
}

static BOOL StringToBool(const char * str)
{
    BOOL ret = FALSE;

    if(strcmp(str, "false") == 0) {
        ret = FALSE;
    } else if(strcmp(str, "true") == 0) {
        ret = TRUE;
    }
    return ret;
}

// Initialize a SIM_Config_prj_type to default values
void SIM_Config_prj_LoadDefaults(SIM_Config_prj_type * aConfig)
{
  aConfig->enable60fps = TRUE;
  aConfig->enable60fpsSpeedFix = TRUE;
    aConfig->overworldWildPokemon = TRUE;
  aConfig->overworldWildDensity = 3;
  aConfig->enable60fpsEverywhere = FALSE;
  aConfig->drawEveryFrame = FALSE;
  aConfig->instantText = TRUE;
  aConfig->instantBars = TRUE;
  aConfig->hmFree = TRUE;
  aConfig->hmNoBadges = FALSE;
  aConfig->hardLevelCap = FALSE;
  aConfig->repelPrompt = TRUE;
  aConfig->followerPokemon = FALSE;
  aConfig->gen5Battles = FALSE;
  aConfig->gen5Screens = FALSE;
  aConfig->expShareAll = FALSE;
  aConfig->clockOffsetMinutes = 0;
  aConfig->nuzlocke = FALSE;
  aConfig->nuzlockeDupes = TRUE;
  aConfig->nuzlockeShiny = TRUE;
  aConfig->nuzlockeToPC = FALSE;
  aConfig->enableAsserts = FALSE;
  aConfig->breakDebuggerOnGfAssert = FALSE;

  // Cheats section
  aConfig->walkThroughWalls = FALSE;
  aConfig->disableRandomEncounters = FALSE;
  aConfig->runFromTrainerBattles = FALSE;
  aConfig->shinyOdds = 0;
  aConfig->expPercent = 100;
  aConfig->alwaysCatch = FALSE;
  aConfig->infinitePP = FALSE;
  aConfig->alwaysEscape = FALSE;
  aConfig->fastEggs = FALSE;
  aConfig->anyTM = FALSE;
    aConfig->catchEmAll = FALSE;
    aConfig->tradeEvos = FALSE;
    aConfig->reusableTMs = FALSE;
    aConfig->infiniteTMs = FALSE;

  // Saves section
  aConfig->saveSlot = 1;
  aConfig->autosave = TRUE;
  aConfig->autosaveMinutes = 5;
  aConfig->autosaveOnExit = TRUE;

  // Gear section
  aConfig->gearEnabled = TRUE;
  aConfig->gearDiscovery = GEAR_DISCOVERY_GUIDE;
  aConfig->gearBattleControls = TRUE;

  // GBA slot section
  aConfig->gbaSlotGame = 0;

  // Touch overlay section
  aConfig->touchOverlay = TRUE;
  aConfig->touchOverlayOpacity = 0.65f;
  aConfig->touchOverlayScale = 1.00f;
  aConfig->topRightOverlay = TRUE;
  aConfig->topRightOverlayScale = 0.36f;
  aConfig->fullScreenCompanion = TRUE;
  aConfig->topRightOverlayMinimized = FALSE;
}

// Loads the configuration file. If it does not exist, return false
BOOL SIM_Config_prj_LoadConfigFile(SIM_Config_prj_type * aConfig)
{
    return (ini_parse(SIM_CONFIG_PRJ_FILE_PATH, IniHandler, aConfig) >= 0);
}

// Save a SIM_Config_prj_type to the configuration file
void SIM_Config_prj_SaveConfigFile(SIM_Config_prj_type * aConfig)
{
    FILE * configFile = fopen(SIM_CONFIG_PRJ_FILE_PATH, "w");
    if (configFile == NULL) {
        return;
    }
    
    fprintf(configFile, "[General]\n");
    fprintf(configFile, "60Fps=%s\n", BoolToString(aConfig->enable60fps));
    fprintf(configFile, "60FpsSpeedFix=%s\n", BoolToString(aConfig->enable60fpsSpeedFix));
    fprintf(configFile, "60FpsEverywhere=%s\n", BoolToString(aConfig->enable60fpsEverywhere));
    fprintf(configFile, "DrawEveryFrame=%s\n", BoolToString(aConfig->drawEveryFrame));
    fprintf(configFile, "InstantText=%s\n", BoolToString(aConfig->instantText));
    fprintf(configFile, "InstantBars=%s\n", BoolToString(aConfig->instantBars));
    fprintf(configFile, "HmFreeFieldMoves=%s\n", BoolToString(aConfig->hmFree));
    fprintf(configFile, "HmNoBadges=%s\n", BoolToString(aConfig->hmNoBadges));
    fprintf(configFile, "HardLevelCap=%s\n", BoolToString(aConfig->hardLevelCap));
    fprintf(configFile, "RepelPrompt=%s\n", BoolToString(aConfig->repelPrompt));
    fprintf(configFile, "FollowerPokemon=%s\n", BoolToString(aConfig->followerPokemon));
    fprintf(configFile, "Gen5Screens=%s\n", BoolToString(aConfig->gen5Screens));
    fprintf(configFile, "Gen5Battles=%s\n", BoolToString(aConfig->gen5Battles));
    fprintf(configFile, "ExpShareAll=%s\n", BoolToString(aConfig->expShareAll));
    fprintf(configFile, "ClockOffsetMinutes=%d\n", aConfig->clockOffsetMinutes);
    fprintf(configFile, "EnableAsserts=%s\n", BoolToString(aConfig->enableAsserts));
    fprintf(configFile, "BreakDebuggerOnAssert=%s\n", BoolToString(aConfig->breakDebuggerOnGfAssert));
    fprintf(configFile, "\n");

    fprintf(configFile, "[Nuzlocke]\n");
    fprintf(configFile, "Enabled=%s\n", BoolToString(aConfig->nuzlocke));
    fprintf(configFile, "DupesClause=%s\n", BoolToString(aConfig->nuzlockeDupes));
    fprintf(configFile, "ShinyClause=%s\n", BoolToString(aConfig->nuzlockeShiny));
    fprintf(configFile, "FaintedToPC=%s\n", BoolToString(aConfig->nuzlockeToPC));
    fprintf(configFile, "\n");

    fprintf(configFile, "[Cheats]\n");
    fprintf(configFile, "WalkThroughWalls=%s\n", BoolToString(aConfig->walkThroughWalls));
    fprintf(configFile, "DisableRandomEncounters=%s\n", BoolToString(aConfig->disableRandomEncounters));
    fprintf(configFile, "RunFromTrainerBattles=%s\n", BoolToString(aConfig->runFromTrainerBattles));
    fprintf(configFile, "ShinyOdds=%d\n", aConfig->shinyOdds);
    fprintf(configFile, "ExpPercent=%d\n", aConfig->expPercent);
    fprintf(configFile, "AlwaysCatch=%s\n", BoolToString(aConfig->alwaysCatch));
    fprintf(configFile, "InfinitePP=%s\n", BoolToString(aConfig->infinitePP));
    fprintf(configFile, "AlwaysEscape=%s\n", BoolToString(aConfig->alwaysEscape));
    fprintf(configFile, "FastEggs=%s\n", BoolToString(aConfig->fastEggs));
    fprintf(configFile, "AnyTM=%s\n", BoolToString(aConfig->anyTM));
    fprintf(configFile, "CatchEmAll=%s\n", BoolToString(aConfig->catchEmAll));
    fprintf(configFile, "TradeEvos=%s\n", BoolToString(aConfig->tradeEvos));
    fprintf(configFile, "ReusableTMs=%s\n", BoolToString(aConfig->reusableTMs));
    fprintf(configFile, "InfiniteTMs=%s\n", BoolToString(aConfig->infiniteTMs));
    fprintf(configFile, "OverworldWildPokemon=%s\n", BoolToString(aConfig->overworldWildPokemon));
    fprintf(configFile, "OverworldWildDensity=%d\n", aConfig->overworldWildDensity);
    fprintf(configFile, "\n");

    fprintf(configFile, "[Saves]\n");
    fprintf(configFile, "Slot=%d\n", aConfig->saveSlot);
    fprintf(configFile, "Autosave=%s\n", BoolToString(aConfig->autosave));
    fprintf(configFile, "AutosaveMinutes=%d\n", aConfig->autosaveMinutes);
    fprintf(configFile, "AutosaveOnExit=%s\n", BoolToString(aConfig->autosaveOnExit));
    fprintf(configFile, "\n");

    fprintf(configFile, "[Gear]\n");
    fprintf(configFile, "Enabled=%s\n", BoolToString(aConfig->gearEnabled));
    fprintf(configFile, "Discovery=%d\n", aConfig->gearDiscovery);
    fprintf(configFile, "BattleControls=%s\n", BoolToString(aConfig->gearBattleControls));
    fprintf(configFile, "\n");

    fprintf(configFile, "[GBA]\n");
    fprintf(configFile, "Game=%d\n", aConfig->gbaSlotGame);

    fprintf(configFile, "[TouchOverlay]\n");
    fprintf(configFile, "Enabled=%s\n", BoolToString(aConfig->touchOverlay));
    fprintf(configFile, "Opacity=%.2f\n", aConfig->touchOverlayOpacity);
    fprintf(configFile, "Scale=%.2f\n", aConfig->touchOverlayScale);
    fprintf(configFile, "TopRightOverlay=%s\n", BoolToString(aConfig->topRightOverlay));
    fprintf(configFile, "TopRightOverlayScale=%.2f\n", aConfig->topRightOverlayScale);
    fprintf(configFile, "FullScreenCompanion=%s\n", BoolToString(aConfig->fullScreenCompanion));
    fprintf(configFile, "TopRightOverlayMinimized=%s\n", BoolToString(aConfig->topRightOverlayMinimized));

    fclose(configFile);
}

static BOOL sConfigLoaded = FALSE;

void SIM_Config_prj_init() {
  sConfigLoaded = TRUE;
  //Load default config
  SIM_Config_prj_LoadDefaults(&s_SIM_config_prj);
  
  //Load configuration file
  if(!SIM_Config_prj_LoadConfigFile(&s_SIM_config_prj)){
    //Config file does not exist. Save config file with defaults
    SIM_Config_prj_SaveConfigFile(&s_SIM_config_prj);
  }
  SIM_SetClockOffset((long long)s_SIM_config_prj.clockOffsetMinutes * 60);
}

SIM_Config_prj_type * SIM_Config_prj_GetConfig()
{
  // libntr can ask before the game starts (the game picker, the lower screen's layout)
  if (!sConfigLoaded) {
    SIM_Config_prj_init();
  }
  return &s_SIM_config_prj;
}
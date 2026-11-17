#include "port/catch_em_all.h"

#include "constants/species.h"
#include "generated/map_headers.h"
#include "port/sim_config_prj.h"
#include "savedata.h"
#include "system_flags.h"

BOOL CatchEmAll_IsEnabled(void)
{
    const SIM_Config_prj_type *config = SIM_Config_prj_GetConfig();
    return config != NULL && config->catchEmAll;
}

static BOOL CatchEmAll_IsPostGame(void)
{
    SaveData *saveData = SaveData_Ptr();
    if (saveData == NULL) {
        return FALSE;
    }
    return SystemFlag_CheckGameCompleted(SaveData_GetVarsFlags(saveData));
}

void CatchEmAll_ApplyGrassSlots(int mapHeaderID, CatchEmAllSlot *slots)
{
    if (!CatchEmAll_IsEnabled() || slots == NULL) {
        return;
    }

    const BOOL postGame = CatchEmAll_IsPostGame();

    switch (mapHeaderID) {
    case MAP_HEADER_ETERNA_FOREST:
        slots[4].species = SPECIES_MURKROW;
        slots[5].species = SPECIES_MISDREAVUS;
        if (postGame) {
            slots[6].species = SPECIES_TURTWIG;
            slots[7].species = SPECIES_TURTWIG;
        }
        break;
    case MAP_HEADER_ROUTE_209_LOST_TOWER_1F:
    case MAP_HEADER_ROUTE_209_LOST_TOWER_2F:
    case MAP_HEADER_ROUTE_209_LOST_TOWER_3F:
    case MAP_HEADER_ROUTE_209_LOST_TOWER_4F:
        slots[4].species = SPECIES_MURKROW;
        slots[5].species = SPECIES_MISDREAVUS;
        break;
    case MAP_HEADER_ROUTE_209_LOST_TOWER_5F:
        slots[4].species = SPECIES_MURKROW;
        slots[5].species = SPECIES_MISDREAVUS;
        slots[10].species = SPECIES_SPIRITOMB;
        break;
    case MAP_HEADER_ROUTE_218:
        slots[4].species = SPECIES_GLAMEOW;
        break;
    case MAP_HEADER_ROUTE_222:
        slots[4].species = SPECIES_GLAMEOW;
        slots[6].species = SPECIES_PURUGLY;
        break;
    case MAP_HEADER_ROUTE_229:
        slots[6].species = SPECIES_PURUGLY;
        break;
    case MAP_HEADER_ROUTE_206:
        slots[4].species = SPECIES_STUNKY;
        break;
    case MAP_HEADER_ROUTE_214:
        slots[4].species = SPECIES_STUNKY;
        slots[6].species = SPECIES_SKUNTANK;
        slots[7].species = SPECIES_POOCHYENA;
        break;
    case MAP_HEADER_ROUTE_225:
        slots[6].species = SPECIES_SKUNTANK;
        break;
    case MAP_HEADER_ROUTE_228:
        slots[6].species = SPECIES_TRAPINCH;
        slots[8].species = SPECIES_VIBRAVA;
        break;
    case MAP_HEADER_ROUTE_205_NORTH:
        slots[10].species = SPECIES_MUNCHLAX;
        break;
    case MAP_HEADER_ROUTE_210_SOUTH:
        slots[8].species = SPECIES_HERACROSS;
        break;
    case MAP_HEADER_OLD_CHATEAU:
        slots[8].species = SPECIES_ROTOM;
        slots[10].species = SPECIES_SPIRITOMB;
        break;
    case MAP_HEADER_ROUTE_204_SOUTH:
        slots[6].species = SPECIES_LOTAD;
        slots[7].species = SPECIES_SEEDOT;
        break;
    case MAP_HEADER_ROUTE_204_NORTH:
        slots[6].species = SPECIES_PINECO;
        break;
    case MAP_HEADER_ROUTE_205_SOUTH:
        slots[6].species = SPECIES_SLOWPOKE;
        break;
    case MAP_HEADER_ROUTE_207:
        slots[6].species = SPECIES_STANTLER;
        break;
    case MAP_HEADER_ROUTE_208:
        slots[6].species = SPECIES_ZANGOOSE;
        slots[7].species = SPECIES_SEVIPER;
        break;
    case MAP_HEADER_ROUTE_209:
        slots[6].species = SPECIES_VULPIX;
        slots[7].species = SPECIES_GROWLITHE;
        break;
    case MAP_HEADER_ROUTE_210_NORTH:
        slots[6].species = SPECIES_KECLEON;
        break;
    case MAP_HEADER_ROUTE_211_EAST:
        slots[6].species = SPECIES_TEDDIURSA;
        break;
    case MAP_HEADER_ROUTE_212_SOUTH:
        slots[6].species = SPECIES_EKANS;
        break;
    case MAP_HEADER_WAYWARD_CAVE_1F:
        slots[6].species = SPECIES_SANDSHREW;
        break;
    case MAP_HEADER_FUEGO_IRONWORKS_OUTSIDE:
        slots[6].species = SPECIES_ARON;
        break;
    case MAP_HEADER_LAKE_VERITY:
        slots[6].species = SPECIES_LUNATONE;
        break;
    case MAP_HEADER_LAKE_VALOR:
        slots[6].species = SPECIES_SOLROCK;
        break;
    case MAP_HEADER_LAKE_ACUITY:
        slots[6].species = SPECIES_URSARING;
        break;
    case MAP_HEADER_ROUTE_227:
    case MAP_HEADER_STARK_MOUNTAIN_OUTSIDE:
        if (postGame) {
            slots[6].species = SPECIES_CHIMCHAR;
            slots[7].species = SPECIES_CHIMCHAR;
        }
        break;
    case MAP_HEADER_ACUITY_LAKEFRONT:
        if (postGame) {
            slots[6].species = SPECIES_PIPLUP;
            slots[7].species = SPECIES_PIPLUP;
        }
        break;
    }
}

void CatchEmAll_ApplySurfSlots(int mapHeaderID, CatchEmAllSlot *slots)
{
    if (!CatchEmAll_IsEnabled() || slots == NULL) {
        return;
    }
    if (mapHeaderID == MAP_HEADER_ROUTE_223 && CatchEmAll_IsPostGame()) {
        slots[2].species = SPECIES_PIPLUP;
        slots[3].species = SPECIES_PIPLUP;
    }
}

void CatchEmAll_ApplySuperRodSlots(int mapHeaderID, CatchEmAllSlot *slots)
{
    if (!CatchEmAll_IsEnabled() || slots == NULL) {
        return;
    }
    if (mapHeaderID == MAP_HEADER_ROUTE_219 || mapHeaderID == MAP_HEADER_ROUTE_221) {
        slots[2].species = SPECIES_CLAMPERL;
    }
}

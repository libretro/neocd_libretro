#include <vector>

#include "libretro_bios.h"
#include "libretro_common.h"
#include "libretro_variables.h"
#include "libretro.h"
#include "neogeocd.h"

// Variable names for the settings
static const char* const REGION_VARIABLE = "neocd_region";
static const char* const BIOS_VARIABLE = "neocd_bios";
static const char* const SPEEDHACK_VARIABLE = "neocd_cdspeedhack";
static const char* const LOADSKIP_VARIABLE = "neocd_loadskip";
static const char* const PER_CONTENT_SAVES_VARIABLE = "neocd_per_content_saves";

// All core variables
static std::vector<retro_variable> variables;

void Libretro::Variables::init()
{
    variables.clear();

    variables.emplace_back(retro_variable{ REGION_VARIABLE, "Region; Japan|USA|Europe" });

    globals.biosChoices.clear();

    if (globals.biosList.size())
    {
        globals.biosChoices = "BIOS Select; ";
        globals.biosChoices.append(globals.biosList[0].description);

        for (size_t i = 1; i < globals.biosList.size(); ++i)
        {
            globals.biosChoices.append("|");
            globals.biosChoices.append(globals.biosList[i].description);
        }

        variables.emplace_back(retro_variable{ BIOS_VARIABLE, globals.biosChoices.c_str() });
    }

    variables.emplace_back(retro_variable{ SPEEDHACK_VARIABLE, "CD Speed Hack; On|Off" });

    variables.emplace_back(retro_variable{ LOADSKIP_VARIABLE, "Skip CD Loading; On|Off" });

    variables.emplace_back(retro_variable{ PER_CONTENT_SAVES_VARIABLE, "Per-Game Saves (Restart); Off|On" });

    variables.emplace_back(retro_variable{ nullptr, nullptr });

    libretro.environment(RETRO_ENVIRONMENT_SET_VARIABLES, reinterpret_cast<void*>(&variables[0]));
}

void Libretro::Variables::update(bool needReset)
{
    struct retro_variable var;

    var.value = NULL;
    var.key = REGION_VARIABLE;

    if (libretro.environment(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        uint32_t nationality = NeoGeoCD::NationalityJapan;

        if (!strcmp(var.value, "USA"))
            nationality = NeoGeoCD::NationalityUSA;
        else if (!strcmp(var.value, "Europe"))
            nationality = NeoGeoCD::NationalityEurope;

        if (neocd->machineNationality != nationality)
        {
            neocd->machineNationality = nationality;
            needReset = true;
        }
    }

    var.value = NULL;
    var.key = BIOS_VARIABLE;

    if (libretro.environment(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        size_t index = Libretro::Bios::descriptionToIndex(var.value);

        if (index != globals.biosIndex)
        {
            globals.biosIndex = index;
            Libretro::Bios::load();
            needReset = true;
        }
    }

    var.value = NULL;
    var.key = SPEEDHACK_VARIABLE;

    if (libretro.environment(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        bool newValue = strcmp(var.value, "On") ? false : true;
        if (globals.cdSpeedHack != newValue)
        {
            globals.cdSpeedHack = newValue;
            Libretro::Bios::load();
            needReset = true;
        }
    }

    var.value = NULL;
    var.key = LOADSKIP_VARIABLE;

    if (libretro.environment(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
        globals.skipCDLoading = strcmp(var.value, "On") ? false : true;

    var.value = NULL;
    var.key = PER_CONTENT_SAVES_VARIABLE;

    if (libretro.environment(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
        globals.perContentSaves = strcmp(var.value, "On") ? false : true;

    if (needReset)
        neocd->reset();
}

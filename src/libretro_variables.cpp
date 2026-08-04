#include <cstdlib>
#include <vector>

#include "libretro_bios.h"
#include "libretro_common.h"
#include "libretro_variables.h"
#include "libretro.h"
#include "neogeocd.h"
#include "video.h"

// Variable names for the settings
static const char* const REGION_VARIABLE = "neocd_region";
static const char* const BIOS_VARIABLE = "neocd_bios";
static const char* const SPEEDHACK_VARIABLE = "neocd_cdspeedhack";
static const char* const LOADSKIP_VARIABLE = "neocd_loadskip";
static const char* const PER_CONTENT_SAVES_VARIABLE = "neocd_per_content_saves";
static const char* const OVERSCAN_H_VARIABLE = "neocd_overscan_h";
static const char* const CPU_OVERCLOCK_VARIABLE = "neocd_cpu_overclock";

static const char* const CATEGORY_SYSTEM = "system";
static const char* const CATEGORY_VIDEO = "video";
static const char* const CATEGORY_AUDIO = "audio";
static const char* const CATEGORY_INPUT = "input";
static const char* const CATEGORY_ADVANCED = "advanced";

// All core variables
static std::vector<retro_variable> variables;
static std::vector<retro_core_option_v2_definition> coreOptionDefinitions;

static retro_core_option_v2_category coreOptionCategories[] = {
    { CATEGORY_SYSTEM, "System", nullptr },
    { CATEGORY_VIDEO, "Video", nullptr },
    //{ CATEGORY_AUDIO, "Audio", nullptr },
    //{ CATEGORY_INPUT, "Input", nullptr },
    { CATEGORY_ADVANCED, "Advanced", nullptr },
    { nullptr, nullptr, nullptr },
};

static retro_core_options_v2 coreOptionsV2 = { coreOptionCategories, nullptr };

static void buildBiosChoices()
{
    globals.biosChoices.clear();

    if (globals.biosList.empty())
        return;

    globals.biosChoices = "BIOS Select; ";
    globals.biosChoices.append(globals.biosList[0].description);

    for (size_t i = 1; i < globals.biosList.size(); ++i)
    {
        globals.biosChoices.append("|");
        globals.biosChoices.append(globals.biosList[i].description);
    }
}

static void fillBasicOption(retro_core_option_v2_definition& option,
                            const char* key,
                            const char* desc,
                            const char* categoryKey,
                            const char* defaultValue,
                            const char* const* values,
                            size_t valueCount)
{
    option = retro_core_option_v2_definition{};
    option.key = key;
    option.desc = desc;
    option.desc_categorized = desc;
    option.category_key = categoryKey;

    const size_t maxValues = RETRO_NUM_CORE_OPTION_VALUES_MAX - 1;
    const size_t count = valueCount > maxValues ? maxValues : valueCount;

    for (size_t i = 0; i < count; ++i)
    {
        option.values[i].value = values[i];
        option.values[i].label = values[i];
    }

    option.values[count].value = NULL;
    option.values[count].label = NULL;
    option.default_value = defaultValue;
}

static void fillBiosOption(retro_core_option_v2_definition& option)
{
    option = retro_core_option_v2_definition{};
    option.key = BIOS_VARIABLE;
    option.desc = "BIOS Select";
    option.desc_categorized = "BIOS Select";
    option.category_key = CATEGORY_SYSTEM;

    size_t count = globals.biosList.size();
    const size_t maxValues = RETRO_NUM_CORE_OPTION_VALUES_MAX - 1;
    if (count > maxValues)
        count = maxValues;

    for (size_t i = 0; i < count; ++i)
    {
        const char* desc = globals.biosList[i].description.c_str();
        option.values[i].value = desc;
        option.values[i].label = desc;
    }

    option.values[count].value = NULL;
    option.values[count].label = NULL;
    option.default_value = count ? option.values[0].value : NULL;
}

static void buildLegacyVariables()
{
    variables.clear();

    variables.emplace_back(retro_variable{ REGION_VARIABLE, "Region; Japan|USA|Europe" });

    buildBiosChoices();

    if (globals.biosList.size())
        variables.emplace_back(retro_variable{ BIOS_VARIABLE, globals.biosChoices.c_str() });

    variables.emplace_back(retro_variable{ OVERSCAN_H_VARIABLE, "Horizontal Overscan Mask; 8|4|0|12|16" });
    variables.emplace_back(retro_variable{ SPEEDHACK_VARIABLE, "CD Speed Hack; On|Off" });
    variables.emplace_back(retro_variable{ CPU_OVERCLOCK_VARIABLE, "CPU Overclock; 100%|110%|125%|150%|200%" });
    variables.emplace_back(retro_variable{ LOADSKIP_VARIABLE, "Skip CD Loading; On|Off" });
    variables.emplace_back(retro_variable{ PER_CONTENT_SAVES_VARIABLE, "Per-Game Saves (Restart); Off|On" });

    variables.emplace_back(retro_variable{ nullptr, nullptr });
}

static void buildCoreOptionsV2()
{
    coreOptionDefinitions.clear();
    coreOptionDefinitions.reserve(7);

    retro_core_option_v2_definition option;

    const char* const regionValues[] = { "Japan", "USA", "Europe" };
    fillBasicOption(option, REGION_VARIABLE, "Region", CATEGORY_SYSTEM, "Japan", regionValues, 3);
    coreOptionDefinitions.emplace_back(option);

    if (!globals.biosList.empty())
    {
        fillBiosOption(option);
        coreOptionDefinitions.emplace_back(option);
    }

    const char* const overscanValues[] = { "8", "4", "0", "12", "16" };
    fillBasicOption(option, OVERSCAN_H_VARIABLE, "Horizontal Overscan Mask", CATEGORY_VIDEO, "8", overscanValues, 5);
    coreOptionDefinitions.emplace_back(option);

    const char* const onOffValues[] = { "On", "Off" };
    fillBasicOption(option, SPEEDHACK_VARIABLE, "CD Speed Hack", CATEGORY_ADVANCED, "On", onOffValues, 2);
    coreOptionDefinitions.emplace_back(option);

    fillBasicOption(option, LOADSKIP_VARIABLE, "Skip CD Loading", CATEGORY_ADVANCED, "On", onOffValues, 2);
    coreOptionDefinitions.emplace_back(option);

    const char* const overclockValues[] = { "100%", "110%", "125%", "150%", "200%" };
    fillBasicOption(option, CPU_OVERCLOCK_VARIABLE, "CPU Overclock", CATEGORY_ADVANCED, "100%", overclockValues, 5);
    coreOptionDefinitions.emplace_back(option);

    const char* const offOnValues[] = { "Off", "On" };
    fillBasicOption(option, PER_CONTENT_SAVES_VARIABLE, "Per-Game Saves (Restart)", CATEGORY_SYSTEM, "Off", offOnValues, 2);
    coreOptionDefinitions.emplace_back(option);

    coreOptionDefinitions.emplace_back(retro_core_option_v2_definition{});
    coreOptionsV2.definitions = coreOptionDefinitions.data();
}

void Libretro::Variables::init()
{
    unsigned optionsVersion = 0;
    bool setOptions = false;

    if (libretro.environment(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &optionsVersion) && optionsVersion >= 2)
    {
        buildCoreOptionsV2();
        setOptions = libretro.environment(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &coreOptionsV2);
    }

    if (!setOptions)
    {
        buildLegacyVariables();
        libretro.environment(RETRO_ENVIRONMENT_SET_VARIABLES, reinterpret_cast<void*>(&variables[0]));
    }
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
    var.key = OVERSCAN_H_VARIABLE;

    if (libretro.environment(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        uint32_t newValue = static_cast<uint32_t>(atoi(var.value));

        if (newValue > 16)
            newValue = 16;

        if (globals.overscanH != newValue)
        {
            globals.overscanH = newValue;

            struct retro_game_geometry geometry;
            geometry.base_width = Video::FRAMEBUFFER_WIDTH - (globals.overscanH * 2);
            geometry.base_height = Video::FRAMEBUFFER_HEIGHT;
            geometry.max_width = Video::FRAMEBUFFER_WIDTH;
            geometry.max_height = Video::FRAMEBUFFER_HEIGHT;
            geometry.aspect_ratio = Video::ASPECT_RATIO;

            libretro.environment(RETRO_ENVIRONMENT_SET_GEOMETRY, &geometry);
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
    var.key = CPU_OVERCLOCK_VARIABLE;

    if (libretro.environment(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        uint32_t newValue = (uint32_t)atoi(var.value);
        if (newValue >= 100 && newValue <= 1000)
            globals.cpuOverclock = newValue;
    }

    var.value = NULL;
    var.key = PER_CONTENT_SAVES_VARIABLE;

    if (libretro.environment(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
        globals.perContentSaves = strcmp(var.value, "On") ? false : true;

    if (needReset)
        neocd->reset();
}

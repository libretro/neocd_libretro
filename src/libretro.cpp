#include "neogeocd.h"
#include "timeprofiler.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

class BiosListEntry
{
public:
    BiosListEntry(const std::string& f, const std::string& d, NeoGeoCD::BiosType t) :
        filename(f),
        description(d),
        type(t)
    { }

    std::string         filename;
    std::string         description;
    NeoGeoCD::BiosType type;
};

static const char* BIOS_F_FILENAME = "neocd_f.rom";
static const char* BIOS_F_DESCRIPTION = "Front Loader";
static const char* BIOS_SF_FILENAME = "neocd_sf.rom";
static const char* BIOS_SF_DESCRIPTION = "Front Loader (SMKDAN)";
static const char* BIOS_T_FILENAME = "neocd_t.rom";
static const char* BIOS_T_DESCRIPTION = "Top Loader";
static const char* BIOS_ST_FILENAME = "neocd_st.rom";
static const char* BIOS_ST_DESCRIPTION = "Top Loader (SMKDAN)";
static const char* BIOS_Z_FILENAME = "neocd_z.rom";
static const char* BIOS_Z_DESCRIPTION = "CDZ";
static const char* BIOS_SZ_FILENAME = "neocd_sz.rom";
static const char* BIOS_SZ_DESCRIPTION = "CDZ (SMKDAN)";

static const char* REGION_VARIABLE = "neocd_region";
static const char* BIOS_VARIABLE = "neocd_bios";
static const char* SPEEDHACK_VARIABLE = "neocd_cdspeedhack";
static const char* LOADSKIP_VARIABLE = "neocd_loadskip";

static const struct retro_input_descriptor neogeoCDPadDescriptors[] = {
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "A" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "B" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "C" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "D" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },

    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "A" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "B" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "C" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "D" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
    { 0 }
};

static const uint8_t padMap[] = {
    RETRO_DEVICE_ID_JOYPAD_LEFT, Input::Left,
    RETRO_DEVICE_ID_JOYPAD_UP, Input::Up,
    RETRO_DEVICE_ID_JOYPAD_DOWN, Input::Down,
    RETRO_DEVICE_ID_JOYPAD_RIGHT, Input::Right,
    RETRO_DEVICE_ID_JOYPAD_B, Input::A,
    RETRO_DEVICE_ID_JOYPAD_A, Input::B,
    RETRO_DEVICE_ID_JOYPAD_Y, Input::C,
    RETRO_DEVICE_ID_JOYPAD_X, Input::D
};

static const uint8_t padMap2[] = {
    0, RETRO_DEVICE_ID_JOYPAD_START, Input::Controller1Start,
    0, RETRO_DEVICE_ID_JOYPAD_SELECT, Input::Controller1Select,
    1, RETRO_DEVICE_ID_JOYPAD_START, Input::Controller2Start,
    1, RETRO_DEVICE_ID_JOYPAD_SELECT, Input::Controller2Select
};

static const char* systemDirectory = nullptr;

LibretroCallbacks libretro = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

static std::vector<BiosListEntry> biosList;
static int biosIndex = 0;
static std::string biosChoices;

static bool skipCDLoading = true;
static bool cdSpeedHack = false;

static std::vector<retro_variable> variables;

static std::string makeSystemPath(const std::string& filename)
{
    std::string result;

    if (systemDirectory)
        result = std::string(systemDirectory) + "/neocd/" + filename;
    else
        result = filename;

    return result;
}

static bool fileExists(const std::string& filename)
{
    std::ifstream file;
    file.open(filename);
    if (file.is_open())
        return true;

    return false;
}

static void lookForBIOS()
{
    biosList.clear();

    if (fileExists(makeSystemPath(BIOS_F_FILENAME)))
        biosList.emplace_back(BiosListEntry(BIOS_F_FILENAME, BIOS_F_DESCRIPTION, NeoGeoCD::FrontLoader));

    if (fileExists(makeSystemPath(BIOS_SF_FILENAME)))
        biosList.emplace_back(BiosListEntry(BIOS_SF_FILENAME, BIOS_SF_DESCRIPTION, NeoGeoCD::FrontLoader));

    if (fileExists(makeSystemPath(BIOS_T_FILENAME)))
        biosList.emplace_back(BiosListEntry(BIOS_T_FILENAME, BIOS_T_DESCRIPTION, NeoGeoCD::TopLoader));

    if (fileExists(makeSystemPath(BIOS_ST_FILENAME)))
        biosList.emplace_back(BiosListEntry(BIOS_ST_FILENAME, BIOS_ST_DESCRIPTION, NeoGeoCD::TopLoader));

    if (fileExists(makeSystemPath(BIOS_Z_FILENAME)))
        biosList.emplace_back(BiosListEntry(BIOS_Z_FILENAME, BIOS_Z_DESCRIPTION, NeoGeoCD::CDZ));

    if (fileExists(makeSystemPath(BIOS_SZ_FILENAME)))
        biosList.emplace_back(BiosListEntry(BIOS_SZ_FILENAME, BIOS_SZ_DESCRIPTION, NeoGeoCD::CDZ));
}

static bool loadYZoomROM()
{
    std::ifstream file;
    std::string filename;

    filename = makeSystemPath("ng-lo.rom");
    file.open(filename, std::ios::in | std::ios::binary);
    if (!file.is_open())
    {
        LOG(LOG_ERROR, "Could not load Y Zoom ROM %s\n", filename.c_str());
        return false;
    }

    file.read(reinterpret_cast<char*>(neocd.memory.yZoomRom), Memory::YZOOMROM_SIZE);

    if (file.gcount() < Memory::YZOOMROM_SIZE)
    {
        LOG(LOG_ERROR, "ng-lo.rom should be exactly 65536 bytes!\n");
        return false;
    }

    file.close();
    return true;
}

static void patchROM_16(uint32_t address, uint16_t data)
{
    *reinterpret_cast<uint16_t*>(&neocd.memory.rom[address & 0x7FFFF]) = BIG_ENDIAN_WORD(data);
}

static void patchROM_32(uint32_t address, uint32_t data)
{
    *reinterpret_cast<uint32_t*>(&neocd.memory.rom[address & 0x7FFFF]) = BIG_ENDIAN_DWORD(data);
}

static void patchROM_48(uint32_t address, uint32_t data1, uint16_t data2)
{
    *reinterpret_cast<uint32_t*>(&neocd.memory.rom[address & 0x7FFFF]) = BIG_ENDIAN_DWORD(data1);
    address += 4;
    *reinterpret_cast<uint16_t*>(&neocd.memory.rom[address & 0x7FFFF]) = BIG_ENDIAN_WORD(data2);
}

static void installSpeedHack(uint32_t address)
{
    patchROM_16(address, 0xFABE);
}

static void disableSMKDANChecksum(uint32_t address)
{
    patchROM_48(address, 0x22004E71, 0x4E71);
}

static void patchBIOS()
{
    bool isSMKDan = (biosList[biosIndex].filename == BIOS_SZ_FILENAME)
                    || (biosList[biosIndex].filename == BIOS_SF_FILENAME)
                    || (biosList[biosIndex].filename == BIOS_ST_FILENAME);

    if (neocd.biosType == NeoGeoCD::FrontLoader)
    {
        // Speed hacks to avoid busy looping
        if (cdSpeedHack)
        {
            installSpeedHack(0xC10716);
            installSpeedHack(0xC10758);
            installSpeedHack(0xC10798);
            installSpeedHack(0xC10864);
        }

        // If SMKDan, disable the BIOS checksum
        if (isSMKDan)
            disableSMKDANChecksum(0xC23EBE);
    }
    else if (neocd.biosType == NeoGeoCD::TopLoader)
    {
        // Speed hacks to avoid busy looping
        if (cdSpeedHack)
        {         
            installSpeedHack(0xC0FFCA); // b
            installSpeedHack(0xC1000E); // a
            installSpeedHack(0xC1004E); // b
            installSpeedHack(0xC10120); // b
        }

        // If SMKDan, disable the BIOS checksum
        if (isSMKDan)
            disableSMKDANChecksum(0xC23FBE);
    }
    else if (neocd.biosType == NeoGeoCD::CDZ)
    {
        // Patch the CD Recognition (It's done in the CD-ROM firmware, can't emulate)
        patchROM_16(0xC0D280, 0x4E71);

        // Speed hacks to avoid busy looping
        if (cdSpeedHack)
        {
            installSpeedHack(0xC0E6E0);
            installSpeedHack(0xC0E724);
            installSpeedHack(0xC0E764);
            installSpeedHack(0xC0E836);
            installSpeedHack(0xC0E860);
        }

        // If SMKDan, disable the BIOS checksum
        if (isSMKDan)
            disableSMKDANChecksum(0xC62BF4);
    }
}

static bool loadBIOS()
{
    std::ifstream file;
    std::string filename;

    if (!biosList.size())
    {
        LOG(LOG_ERROR, "No BIOS detected!\n");
        return false;
    }

    filename = makeSystemPath(biosList[biosIndex].filename);
    file.open(filename, std::ios::in | std::ios::binary);
    if (!file.is_open())
    {
        LOG(LOG_ERROR, "Could not load BIOS %s\n", filename.c_str());
        return false;
    }

    file.read(reinterpret_cast<char*>(neocd.memory.rom), Memory::ROM_SIZE);

    if (file.gcount() < Memory::ROM_SIZE)
    {
        LOG(LOG_ERROR, "neocd.rom should be exactly 524288 bytes!\n");
        return false;
    }

    file.close();

    neocd.biosType = biosList[biosIndex].type;

    // Swab the BIOS if needed
    if (*reinterpret_cast<uint16_t*>(&neocd.memory.rom[0]) == 0x0010)
    {
        uint16_t* start = reinterpret_cast<uint16_t*>(&neocd.memory.rom[0]);
        uint16_t* end = reinterpret_cast<uint16_t*>(&neocd.memory.rom[Memory::ROM_SIZE]);

        std::for_each(start, end, [](uint16_t& data) {
            data = BIG_ENDIAN_WORD(data);
        });
    }

    patchBIOS();

    return true;
}

static int biosDescriptionToIndex(const char* description)
{
    for (int result = 0; result < biosList.size(); ++result)
    {
        if (!strcmp(description, biosList[result].description.c_str()))
            return result;
    }

    return 0;
}

static void buildVariableList()
{
    variables.clear();

    variables.emplace_back(retro_variable{ REGION_VARIABLE, "Region; Japan|USA|Europe" });
    
    biosChoices.clear();
    
    if (biosList.size())
    {
        biosChoices = "BIOS Select; ";
        biosChoices.append(biosList[0].description);

        for (size_t i = 1; i < biosList.size(); ++i)
        {
            biosChoices.append("|");
            biosChoices.append(biosList[i].description);
        }

        variables.emplace_back(retro_variable{ BIOS_VARIABLE, biosChoices.c_str() });
    }

    variables.emplace_back(retro_variable{ SPEEDHACK_VARIABLE, "CD Speed Hack; On|Off" });

    variables.emplace_back(retro_variable{ LOADSKIP_VARIABLE, "Skip CD Loading; On|Off" });

    variables.emplace_back(retro_variable{ nullptr, nullptr });
}

static void loadBackupRam()
{
    std::ifstream file;
    std::string filename;

    filename = makeSystemPath("neocd.srm");
    file.open(filename, std::ios::in | std::ios::binary);
    if (!file.is_open())
        return;

    file.read(reinterpret_cast<char*>(neocd.memory.backupRam), Memory::BACKUPRAM_SIZE);
    file.close();
}

static void saveBackupRam()
{
    std::ofstream file;
    std::string filename;

    filename = makeSystemPath("neocd.srm");
    file.open(filename, std::ios::out | std::ios::binary);
    if (!file.is_open())
        return;

    file.write(reinterpret_cast<char*>(neocd.memory.backupRam), Memory::BACKUPRAM_SIZE);
    file.close();
}

static void updateVariables(bool needReset)
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

        if (neocd.machineNationality != nationality)
        {
            neocd.machineNationality = nationality;
            needReset = true;
        }
    }

    var.value = NULL;
    var.key = BIOS_VARIABLE;
    
    if (libretro.environment(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        int index = biosDescriptionToIndex(var.value);

        if (index != biosIndex)
        {
            biosIndex = index;
            loadBIOS();
            needReset = true;
        }
    }

    var.value = NULL;
    var.key = SPEEDHACK_VARIABLE;
    
    if (libretro.environment(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        bool newValue = strcmp(var.value, "On") ? false : true;
        if (cdSpeedHack != newValue)
        {
            cdSpeedHack = newValue;
            loadBIOS();
            needReset = true;
        }
    }

    var.value = NULL;
    var.key = LOADSKIP_VARIABLE;
    
    if (libretro.environment(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
        skipCDLoading = strcmp(var.value, "On") ? false : true;
        
    if (needReset)
        neocd.reset();
}

void retro_set_environment(retro_environment_t cb)
{
    libretro.environment = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
    libretro.video = cb;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
    UNUSED_ARG(cb);
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
    libretro.audioBatch = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
    libretro.inputPoll = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
    libretro.inputState = cb;
}

unsigned retro_api_version(void)
{
    return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
    UNUSED_ARG(port);
    UNUSED_ARG(device);
}

void retro_get_system_info(struct retro_system_info *info)
{
    std::memset(info, 0, sizeof(retro_system_info));
    
    info->library_name = "NeoCD";
    info->library_version = "2018";
    info->valid_extensions = "cue";
    info->need_fullpath = true;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
    std::memset(info, 0, sizeof(retro_system_av_info));
    
    info->timing.fps = Timer::FRAME_RATE;
    info->timing.sample_rate = static_cast<double>(Audio::SAMPLE_RATE);
    info->geometry.base_width = Video::FRAMEBUFFER_WIDTH;
    info->geometry.base_height = Video::FRAMEBUFFER_HEIGHT;
    info->geometry.max_width = Video::FRAMEBUFFER_WIDTH;
    info->geometry.max_height = Video::FRAMEBUFFER_HEIGHT;
    info->geometry.aspect_ratio = Video::ASPECT_RATIO;
}

size_t retro_serialize_size(void)
{
    static size_t stateSize = 0;

    if (!stateSize)
    {
        DataPacker dummy;
        neocd.saveState(dummy);
        stateSize = dummy.size();
    }

    return stateSize;
}

bool retro_serialize(void *data, size_t size)
{
    if (size < retro_serialize_size())
        return false;

    DataPacker state(reinterpret_cast<char*>(data), 0, size);
    return neocd.saveState(state);
}

bool retro_unserialize(const void *data, size_t size)
{
    if (size < retro_serialize_size())
        return false;

    DataPacker state(const_cast<char*>(reinterpret_cast<const char*>(data)), size, size);
    
    if (neocd.restoreState(state))
        return true;

    loadBIOS();
    neocd.initialize();
    neocd.reset();
    return false;
}

void retro_cheat_reset(void)
{
}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
    UNUSED_ARG(index);
    UNUSED_ARG(enabled);
    UNUSED_ARG(code);
}

bool retro_load_game(const struct retro_game_info *info)
{
    loadBackupRam();

    libretro.environment(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, (void*)neogeoCDPadDescriptors);
    
    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
    if (!libretro.environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
    {
        LOG(LOG_ERROR, "RGB565 support is required!\n");
        return false;
    }
    
    if (!loadYZoomROM())
        return false;

    if (!loadBIOS())
        return false;

    if (!neocd.cdrom.loadCue(info->path))
        return false;

    // Load settings and reset
    updateVariables(true);

    return true;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{
    UNUSED_ARG(game_type);
    UNUSED_ARG(info);
    UNUSED_ARG(num_info);

    return false;
}

void retro_unload_game(void)
{
    saveBackupRam();
}

unsigned retro_get_region(void)
{
    return RETRO_REGION_NTSC;
}

void *retro_get_memory_data(unsigned id)
{
    UNUSED_ARG(id);
    return nullptr;
}

size_t retro_get_memory_size(unsigned id)
{
    UNUSED_ARG(id);
    return 0;
}

void retro_reset(void)
{
    neocd.reset();
}

void retro_run(void)
{
    bool updated = false;

    if (libretro.environment(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
        updateVariables(false);

    uint8_t input1 = 0xFF;
    uint8_t input2 = 0xFF;
    uint8_t input3 = 0x0F;

    PROFILE(p_polling, ProfilingCategory::InputPolling);
    libretro.inputPoll();
    PROFILE_END(p_polling);

    for (int i = 0; i < sizeof(padMap); i += 2)
    {
        if (libretro.inputState(0, RETRO_DEVICE_JOYPAD, 0, padMap[i]))
            input1 &= ~padMap[i + 1];
    }

    for (int i = 0; i < sizeof(padMap); i += 2)
    {
        if (libretro.inputState(1, RETRO_DEVICE_JOYPAD, 0, padMap[i]))
            input2 &= ~padMap[i + 1];
    }

    for (int i = 0; i < sizeof(padMap2); i += 3)
    {
        if (libretro.inputState(padMap2[i], RETRO_DEVICE_JOYPAD, 0, padMap2[i + 1]))
            input3 &= ~padMap2[i + 2];
    }

    neocd.input.setInput(input1, input2, input3);

    // Skip CD loading
    if (neocd.isIRQ1Enabled() && (neocd.lc8951.CTRL0 & LC8951::DECEN) && skipCDLoading)
    {
        neocd.fastForward = true;

        while (neocd.isIRQ1Enabled() || neocd.irq1EnabledThisFrame)
        {
            neocd.irq1EnabledThisFrame = false;
            neocd.runOneFrame();

            if (!neocd.irq1EnabledThisFrame && !(neocd.lc8951.CTRL0 & LC8951::DECEN))
                break;
        }

        neocd.fastForward = false;
    }

    neocd.irq1EnabledThisFrame = false;
    neocd.runOneFrame();

    libretro.audioBatch(neocd.audio.audioBuffer, neocd.audio.samplesThisFrame);
    libretro.video(neocd.video.frameBuffer, Video::FRAMEBUFFER_WIDTH, Video::FRAMEBUFFER_HEIGHT, Video::FRAMEBUFFER_WIDTH * sizeof(uint16_t));
}

void retro_init(void)
{
    struct retro_log_callback log;

    if (libretro.environment(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
        libretro.log = log.log;
    else
        libretro.log = nullptr;
    
    libretro.environment(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &systemDirectory);

    // Initialize the emulation
    neocd.initialize();

    // Init the backup RAM area
    std::memset(neocd.memory.backupRam, 0, sizeof(Memory::BACKUPRAM_SIZE));

    // Search for all supported BIOSes
    lookForBIOS();

    buildVariableList();

    libretro.environment(RETRO_ENVIRONMENT_SET_VARIABLES, reinterpret_cast<void*>(&variables[0]));
}

void retro_deinit(void)
{
    // Deinitialize the emulation
    neocd.deinitialize();

#ifdef PROFILE_ENABLED
    LOG(LOG_INFO, "Total frame time: %f ms\n", g_profilingAccumulators[ProfilingCategory::Total]);
    LOG(LOG_INFO, " CD audio decoding: %f ms\n", g_profilingAccumulators[ProfilingCategory::AudioCD]);
    LOG(LOG_INFO, " YM2610 generation and sound mixing: %f ms\n", g_profilingAccumulators[ProfilingCategory::AudioYM2610]);
    LOG(LOG_INFO, " M68K emulation: %f ms\n", g_profilingAccumulators[ProfilingCategory::CpuM68K]);
    LOG(LOG_INFO, " Z80 emulation: %f ms\n", g_profilingAccumulators[ProfilingCategory::CpuZ80]);
    LOG(LOG_INFO, " Drawing video and handling IRQ: %f ms\n", g_profilingAccumulators[ProfilingCategory::VideoAndIRQ]);
    LOG(LOG_INFO, "Time spent polling inputs: %f ms\n", g_profilingAccumulators[ProfilingCategory::InputPolling]);

    g_profilingAccumulatorsInitialized = false;
#endif
}

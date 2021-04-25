#include <algorithm>
#include <array>
#include <cmath>
#include <compat/posix_string.h>
#include <compat/strl.h>
#include <cstdlib>
#include <cstring>
#include <file/file_path.h>
#include <initializer_list>
#include <lists/dir_list.h>
#include <lists/string_list.h>
#include <vector>

#include "archive.h"
#include "bios.h"
#include "neogeocd.h"
#include "path.h"
#include "stringlist.h"
#include "timeprofiler.h"

#ifdef VITA
#include <psp2/kernel/threadmgr.h>
#endif

enum RetroMapIndex
{
    RAM,
    ROM,
    VRAM,
    Z80,
    BKCP,
    COUNT
};

struct BiosListEntry
{
    std::string filename;
    std::string description;
    Bios::Type type;
};

// Variable names for the settings
static const char* REGION_VARIABLE = "neocd_region";
static const char* BIOS_VARIABLE = "neocd_bios";
static const char* SPEEDHACK_VARIABLE = "neocd_cdspeedhack";
static const char* LOADSKIP_VARIABLE = "neocd_loadskip";
static const char* PER_CONTENT_SAVES_VARIABLE = "neocd_per_content_saves";

// Definition of the Neo Geo arcade stick
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

// Input mapping definition
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

// Input mapping definition
static const uint8_t padMap2[] = {
    0, RETRO_DEVICE_ID_JOYPAD_START, Input::Controller1Start,
    0, RETRO_DEVICE_ID_JOYPAD_SELECT, Input::Controller1Select,
    1, RETRO_DEVICE_ID_JOYPAD_START, Input::Controller2Start,
    1, RETRO_DEVICE_ID_JOYPAD_SELECT, Input::Controller2Select
};

// Retroarch's system directory
const char* systemDirectory = nullptr;

// Retroarch's save directory
const char* saveDirectory = nullptr;

// Path to the srm file
static std::string srmFilename;

// Collection of callbacks to the things we need
LibretroCallbacks libretro = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

// List of all BIOSes found
static std::vector<BiosListEntry> biosList;

// Path to the zoom ROM
static std::string zoomRomFilename;

// Index of currently selected BIOS
static size_t biosIndex = 0;

// Description of all BIOSes separated by |
static std::string biosChoices;

// Should we skip CD loading?
static bool skipCDLoading = true;

// Should we patch the BIOS to lower CPU usage during loading
static bool cdSpeedHack = false;

// All core variables
static std::vector<retro_variable> variables;

// Memory descriptors for cheats, achievements, etc...
static std::array<retro_memory_descriptor, RetroMapIndex::COUNT> memoryDescriptors;

// Memory map for cheats, achievements, etc...
static retro_memory_map memoryMap;

// Load each file from the list and test for validity
static void lookForBIOSInternal(const std::vector<std::string>& fileList)
{
    for(const std::string& path : fileList)
    {
        if (!path_is_bios_file(path.c_str()))
            continue;

        BiosListEntry newEntry;

        std::string archive;
        std::string file;
        split_compressed_path(path, archive, file);

        // For archives we want something of the form archive.zip#file.bin
        if (!archive.empty())
        {
            newEntry.filename = path;
            newEntry.description = make_path_separator(path_basename(archive.c_str()), "#", file.c_str());
        }
        else
        {
            newEntry.filename = file.c_str();
            newEntry.description = path_basename(file.c_str());
        }

        uint8_t buffer[Memory::ROM_SIZE];
        size_t reallyRead;

        if (!Archive::readFile(newEntry.filename, &buffer[0], Memory::ROM_SIZE, &reallyRead))
            continue;

        if (reallyRead != Memory::ROM_SIZE)
            continue;

        Bios::autoByteSwap(&buffer[0]);

        newEntry.type = Bios::identify(&buffer[0]);

        if (newEntry.type.first == Bios::Family::Invalid)
            continue;

        newEntry.description.append(" (");
        newEntry.description.append(Bios::description(newEntry.type));
        newEntry.description.append(")");

        biosList.push_back(newEntry);
    }
}

// Search for BIOS files
static void lookForBIOS()
{
    // Clear everything
    biosList.clear();

    // Get the system path
    const auto systemPath = system_path();

    auto buildFileList = [&]() -> std::vector<std::string>
    {
        std::vector<std::string> result;

        StringList file_list(dir_list_new(systemPath.c_str(), nullptr, false, true, true, true));

        for(const string_list_elem& elem : file_list)
        {
            if (path_is_bios_file(elem.data))
                result.push_back(std::string(elem.data));
            else if (path_is_archive(elem.data))
            {
                auto archiveList = Archive::getFileList(std::string(elem.data));

                for(const std::string& file : archiveList)
                {
                    if (path_is_bios_file(file.c_str()))
                        result.push_back(file);
                }
            }
        }

        return result;
    };

    // Scan the system directory
    auto fileList = buildFileList();

    // Load all files and check for validity
    lookForBIOSInternal(fileList);

    // Sort the list
    std::sort(biosList.begin(), biosList.end(), [](const BiosListEntry& a, const BiosListEntry& b) {
        return strcmp(a.description.c_str(), b.description.c_str()) < 0;
    });
}

// Load, byte swap and patch BIOS
static bool loadBIOS()
{
    if (!biosList.size())
    {
        LOG(LOG_ERROR, "No BIOS detected!\n");
        return false;
    }

    size_t reallyRead;

    if (!Archive::readFile(biosList[biosIndex].filename, neocd.memory.rom, Memory::ROM_SIZE, &reallyRead))
    {
        LOG(LOG_ERROR, "Could not load BIOS %s\n", biosList[biosIndex].filename.c_str());
        return false;
    }

    neocd.biosType = biosList[biosIndex].type.first;

    Bios::autoByteSwap(neocd.memory.rom);

    Bios::patch(neocd.memory.rom, biosList[biosIndex].type, cdSpeedHack);

    return true;
}

static size_t biosDescriptionToIndex(const char* description)
{
    for (size_t result = 0; result < biosList.size(); ++result)
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

    variables.emplace_back(retro_variable{ PER_CONTENT_SAVES_VARIABLE, "Per-Game Saves (Restart); Off|On" });

    variables.emplace_back(retro_variable{ nullptr, nullptr });
}

static void setBackupRamFilename(const char* content_path)
{
    bool per_content_saves = false;
    struct retro_variable var;

    var.value = NULL;
    var.key = PER_CONTENT_SAVES_VARIABLE;

    if (libretro.environment(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
        per_content_saves = strcmp(var.value, "On") ? false : true;

    srmFilename = make_srm_path(per_content_saves, content_path);
}

static void loadBackupRam()
{
    RFILE* file = filestream_open(srmFilename.c_str(), RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
    if (!file)
        return;

    filestream_read(file, neocd.memory.backupRam, Memory::BACKUPRAM_SIZE);
    filestream_close(file);
}

static void saveBackupRam()
{
    RFILE* file = filestream_open(srmFilename.c_str(), RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
    if (!file)
        return;

    filestream_write(file, neocd.memory.backupRam, Memory::BACKUPRAM_SIZE);
    filestream_close(file);
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
        size_t index = biosDescriptionToIndex(var.value);

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

void setRetroMemoryMaps()
{
    memoryDescriptors[RetroMapIndex::RAM]  = { RETRO_MEMDESC_SYSTEM_RAM | RETRO_MEMDESC_BIGENDIAN, reinterpret_cast<void*>(neocd.memory.ram),       0, 0x00000000, 0, 0, Memory::RAM_SIZE,       "RAM" };
    memoryDescriptors[RetroMapIndex::ROM]  = { RETRO_MEMDESC_CONST | RETRO_MEMDESC_BIGENDIAN,      reinterpret_cast<void*>(neocd.memory.rom),       0, 0x00C00000, 0, 0, Memory::ROM_SIZE,       "ROM" };
    
    // Virtual addresses

    memoryDescriptors[RetroMapIndex::VRAM] = { RETRO_MEMDESC_VIDEO_RAM,                            reinterpret_cast<void*>(neocd.memory.videoRam),  0, 0x10000000, 0, 0, Memory::VIDEORAM_SIZE,  "VRAM" };
    memoryDescriptors[RetroMapIndex::Z80]  = { 0,                                                  reinterpret_cast<void*>(neocd.memory.z80Ram),    0, 0x20000000, 0, 0, Memory::Z80RAM_SIZE,    "Z80" };
    memoryDescriptors[RetroMapIndex::BKCP] = { RETRO_MEMDESC_SAVE_RAM,                             reinterpret_cast<void*>(neocd.memory.backupRam), 0, 0x30000000, 0, 0, Memory::BACKUPRAM_SIZE, "BKCP" };

    memoryMap.num_descriptors = RetroMapIndex::COUNT;
    memoryMap.descriptors = memoryDescriptors.data();

    libretro.environment(RETRO_ENVIRONMENT_SET_MEMORY_MAPS, &memoryMap);
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
    info->library_version = "2021";
    info->valid_extensions = "cue|chd";
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
    setBackupRamFilename(info->path);
    loadBackupRam();

    libretro.environment(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, (void*)neogeoCDPadDescriptors);
    
    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
    if (!libretro.environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
    {
        LOG(LOG_ERROR, "RGB565 support is required!\n");
        return false;
    }
    
    if (!loadBIOS())
        return false;

    if (!neocd.cdrom.loadCd(info->path))
    {
        // Call deinitialize to remove the audio thread because Retroarch won't call retro_deinit if load content failed
        neocd.deinitialize();
        return false;
    }

    // Load settings and reset
    updateVariables(true);

    // Set libretro memory maps
    setRetroMemoryMaps();

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

    for (size_t i = 0; i < sizeof(padMap); i += 2)
    {
        if (libretro.inputState(0, RETRO_DEVICE_JOYPAD, 0, padMap[i]))
            input1 &= ~padMap[i + 1];
    }

    for (size_t i = 0; i < sizeof(padMap); i += 2)
    {
        if (libretro.inputState(1, RETRO_DEVICE_JOYPAD, 0, padMap[i]))
            input2 &= ~padMap[i + 1];
    }

    for (size_t i = 0; i < sizeof(padMap2); i += 3)
    {
        if (libretro.inputState(padMap2[i], RETRO_DEVICE_JOYPAD, 0, padMap2[i + 1]))
            input3 &= ~padMap2[i + 2];
    }

    neocd.input.setInput(input1, input2, input3);

    // Skip CD loading
    if (neocd.cdSectorDecodedThisFrame && skipCDLoading)
    {
        neocd.fastForward = true;

        while (neocd.cdSectorDecodedThisFrame)
        {
            neocd.cdSectorDecodedThisFrame = false;
            neocd.runOneFrame();
        }

        neocd.fastForward = false;
    }

    neocd.cdSectorDecodedThisFrame = false;
    neocd.runOneFrame();

    libretro.audioBatch(reinterpret_cast<const int16_t*>(&neocd.audio.buffer.ymSamples[0]), neocd.audio.buffer.sampleCount);
    libretro.video(neocd.video.frameBuffer, Video::FRAMEBUFFER_WIDTH, Video::FRAMEBUFFER_HEIGHT, Video::FRAMEBUFFER_WIDTH * sizeof(uint16_t));
}

void retro_init(void)
{
    struct retro_log_callback log;

    // Initialize the log interface
    if (libretro.environment(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
        libretro.log = log.log;
    else
        libretro.log = nullptr;
    
    // Get the system directory 
    libretro.environment(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &systemDirectory);

    // Get the save directory
    libretro.environment(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &saveDirectory);

    // Initialize the CPU cores
    neocd.initialize();

    // Init the backup RAM area
    std::memset(neocd.memory.backupRam, 0, sizeof(Memory::BACKUPRAM_SIZE));

    // Initialize VFS (If the call fails, fallback versions are used)
    retro_vfs_interface_info vfs_iface_info;
    vfs_iface_info.required_interface_version = 1;
    
    if (libretro.environment(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_iface_info))
    {
        LOG(LOG_INFO, "Using front end provided VFS routines\n");

        filestream_vfs_init(&vfs_iface_info);
        path_vfs_init(&vfs_iface_info);
    }
    else
        LOG(LOG_INFO, "Using fallback VFS routines\n");

    // Search for all supported BIOSes
    lookForBIOS();

    // Set the variables
    buildVariableList();
    libretro.environment(RETRO_ENVIRONMENT_SET_VARIABLES, reinterpret_cast<void*>(&variables[0]));
}

void retro_deinit(void)
{
    LOG(LOG_INFO, "NeoCD deinitializing\n");

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

// stdlibc++ on VITA references usleep but it's not available.
// This is a workaround
#ifdef VITA
extern "C" int usleep(useconds_t usec)
{
   sceKernelDelayThread(usec);
   return 0;
}

extern "C" unsigned int sleep(unsigned int seconds)
{
   sceKernelDelayThread(seconds * 1000000LL);
   return 0;
}
#endif

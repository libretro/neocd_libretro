#include <algorithm>
#include <array>
#include <cmath>
#include <compat/posix_string.h>
#include <compat/strl.h>
#include <cstdlib>
#include <cstring>
#include <file/archive_file.h>
#include <file/file_path.h>
#include <initializer_list>
#include <lists/dir_list.h>
#include <lists/string_list.h>
#include <vector>

#include "neogeocd.h"
#include "path.h"
#include "stringlist.h"
#include "timeprofiler.h"

enum RetroMapIndex
{
    RAM,
    ROM,
    VRAM,
    Z80,
    BKCP,
    COUNT
};

struct KnownBiosListEntry
{
    const char* filename;
    const char* description;
    NeoGeoCD::BiosType type;
    bool isSMKDan;
    bool isUniverse;
};

struct BiosListEntry
{
    std::string filename;
    const KnownBiosListEntry* biosEntry;
};

// List of all known BIOS filenames
static const std::initializer_list<KnownBiosListEntry> KNOWN_BIOSES{
    { "neocd_f.rom", "Front Loader", NeoGeoCD::FrontLoader, false, false },
    { "neocd_sf.rom", "Front Loader (SMKDAN)", NeoGeoCD::FrontLoader, true, false },
    { "front-sp1.bin", "Front Loader (MAME)", NeoGeoCD::FrontLoader, false, false },
    { "neocd_t.rom", "Top Loader", NeoGeoCD::TopLoader, false, false },
    { "neocd_st.rom", "Top Loader (SMKDAN)", NeoGeoCD::TopLoader, true, false },
    { "top-sp1.bin", "Top Loader (MAME)", NeoGeoCD::TopLoader, false, false },
    { "neocd_z.rom", "CDZ", NeoGeoCD::CDZ, false, false },
    { "neocd_sz.rom", "CDZ (SMKDAN)", NeoGeoCD::CDZ, true, false },
    { "neocd.bin", "CDZ (MAME)", NeoGeoCD::CDZ, false, false },
    { "uni-bioscd.rom", "Universe 3.2", NeoGeoCD::CDZ, false, true }
};

// List of all known zoom ROMs
static const std::initializer_list<const char*> KNOWN_ZOOM_ROMS{
    { "ng-lo.rom" },
    { "000-lo.lo" }
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

static void searchForBIOSInternal(const char* path, const StringList& file_list)
{
    for(const string_list_elem& elem : file_list)
    {
        const char* filename = path_basename(elem.data);

        // Check the filename against all known BIOS filenames
        for(const KnownBiosListEntry& entry : KNOWN_BIOSES)
        {
            if (string_compare_insensitive(filename, entry.filename))
            {
                BiosListEntry newEntry;

                // When scanning an archive, path will be non null. 
                // We want something of the form archive.zip#file.bin
                if (path)
                    newEntry.filename = make_path_separator(path, "#", elem.data);
                else
                    newEntry.filename = elem.data;

                newEntry.biosEntry = &entry;
                biosList.push_back(newEntry);

                break;
            }
        }

        // Look for a suitable zoom ROM too
        if (zoomRomFilename.empty())
        {
            for(const char* fname : KNOWN_ZOOM_ROMS)
            {
                if (string_compare_insensitive(fname, filename))
                {
                    if (path)
                        zoomRomFilename = make_path_separator(path, "#", elem.data);
                    else
                        zoomRomFilename = elem.data;

                    break;
                }
            }
        }
    }
}

static void lookForBIOS()
{
    const char* const valid_exts = "rom|bin|lo";

    // Clear everything
    biosList.clear();
    zoomRomFilename.clear();

    // Get the system path
    std::string systemPath = system_path();

    // Scan the system directory
    StringList file_list(dir_list_new(systemPath.c_str(), valid_exts, false, false, false, true));
    searchForBIOSInternal(nullptr, file_list);

    // Get a list of all archives present in the system directory...
    StringList archive_list(dir_list_new(systemPath.c_str(), "", false, false, true, true));
    for(const string_list_elem& elem : archive_list)
    {
        // ...and scan them too
        file_list = file_archive_get_file_list(elem.data, nullptr);
        searchForBIOSInternal(elem.data, file_list);
    }

    // Sort the list
    std::sort(biosList.begin(), biosList.end(), [](const BiosListEntry& a, const BiosListEntry& b) {
        return strcmp(a.biosEntry->description, b.biosEntry->description) < 0;
    });

    // Make the list unique
    auto i = std::unique(biosList.begin(), biosList.end(), [](const BiosListEntry& a, const BiosListEntry& b) {
        return strcmp(a.biosEntry->description, b.biosEntry->description) == 0;
    });
    biosList.erase(i, biosList.end());
}

static bool fileRead(const std::string& path, void* buffer, size_t maximumSize, size_t* reallyRead)
{
    size_t wasRead = 0;

    if (!path_contains_compressed_file(path.c_str()))
    {
        RFILE* file = filestream_open(path.c_str(), RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
        if (!file)
            return false;

        wasRead = filestream_read(file, buffer, maximumSize);

        filestream_close(file);
    }
    else
    {
        void* tempBuffer;
        int64_t size;

        if (!file_archive_compressed_read(path.c_str(), &tempBuffer, nullptr, &size))
            return false;

        wasRead = std::min(maximumSize, static_cast<size_t>(size));

        std::memcpy(buffer, tempBuffer, wasRead);

        std::free(tempBuffer);
    }

    if (reallyRead)
        *reallyRead = wasRead;

    return true;
}

static bool loadYZoomROM()
{
    size_t reallyRead;

    if (!fileRead(zoomRomFilename, neocd.memory.yZoomRom, Memory::YZOOMROM_SIZE, &reallyRead))
    {
        LOG(LOG_ERROR, "Could not load Y Zoom ROM\n");
        return false;
    }

    if (reallyRead < Memory::YZOOMROM_SIZE)
    {
        LOG(LOG_ERROR, "Y ZOOM ROM should be at least 65536 bytes!\n");
        return false;
    }

    return true;
}

static void patchROM_16(uint32_t address, uint16_t data)
{
    *reinterpret_cast<uint16_t*>(&neocd.memory.rom[address & 0x7FFFF]) = BIG_ENDIAN_WORD(data);
}

/*static void patchROM_32(uint32_t address, uint32_t data)
{
    *reinterpret_cast<uint32_t*>(&neocd.memory.rom[address & 0x7FFFF]) = BIG_ENDIAN_DWORD(data);
}*/

static void patchROM_48(uint32_t address, uint32_t data1, uint16_t data2)
{
    *reinterpret_cast<uint32_t*>(&neocd.memory.rom[address & 0x7FFFF]) = BIG_ENDIAN_DWORD(data1);
    address += 4;
    *reinterpret_cast<uint16_t*>(&neocd.memory.rom[address & 0x7FFFF]) = BIG_ENDIAN_WORD(data2);
}

static void installSpeedHack(uint32_t address)
{
    patchROM_48(address, 0xFABE4E71, 0x4E71);
}

static void disableSMKDANChecksum(uint32_t address)
{
    patchROM_48(address, 0x22004E71, 0x4E71);
}

static void patchBIOS()
{
    const KnownBiosListEntry* entry = biosList[biosIndex].biosEntry;

    if (neocd.biosType == NeoGeoCD::FrontLoader)
    {
        // Patch the CD Recognition
        patchROM_16(0xC10B64, 0x4E71);

        // Speed hacks to avoid busy looping
        if (cdSpeedHack)
        {
            installSpeedHack(0xC10716);
            installSpeedHack(0xC10758);
            installSpeedHack(0xC10798);
            installSpeedHack(0xC10864);
        }

        // If SMKDan, disable the BIOS checksum
        if (entry->isSMKDan)
            disableSMKDANChecksum(0xC23EBE);
    }
    else if (neocd.biosType == NeoGeoCD::TopLoader)
    {
        // Patch the CD Recognition
        patchROM_16(0xC10436, 0x4E71);
        
        // Speed hacks to avoid busy looping
        if (cdSpeedHack)
        {         
            installSpeedHack(0xC0FFCA); // b
            installSpeedHack(0xC1000E); // a
            installSpeedHack(0xC1004E); // b
            installSpeedHack(0xC10120); // b
        }

        // If SMKDan, disable the BIOS checksum
        if (entry->isSMKDan)
            disableSMKDANChecksum(0xC23FBE);
    }
    else if (neocd.biosType == NeoGeoCD::CDZ)
    {
        // Patch the CD Recognition
        patchROM_16(0xC0EB82, 0x4E71);
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
        if (entry->isSMKDan)
            disableSMKDANChecksum(0xC62BF4);
    }
}

static bool loadBIOS()
{
    if (!biosList.size())
    {
        LOG(LOG_ERROR, "No BIOS detected!\n");
        return false;
    }

    size_t reallyRead;

    if (!fileRead(biosList[biosIndex].filename, neocd.memory.rom, Memory::ROM_SIZE, &reallyRead))
    {
        LOG(LOG_ERROR, "Could not load BIOS %s\n", biosList[biosIndex].filename.c_str());
        return false;
    }

    if (reallyRead < Memory::ROM_SIZE)
    {
        LOG(LOG_ERROR, "neocd.rom should be exactly 524288 bytes!\n");
        return false;
    }

    neocd.biosType = biosList[biosIndex].biosEntry->type;

    // Swap the BIOS if needed
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

static size_t biosDescriptionToIndex(const char* description)
{
    for (size_t result = 0; result < biosList.size(); ++result)
    {
        if (!strcmp(description, biosList[result].biosEntry->description))
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
        biosChoices.append(biosList[0].biosEntry->description);

        for (size_t i = 1; i < biosList.size(); ++i)
        {
            biosChoices.append("|");
            biosChoices.append(biosList[i].biosEntry->description);
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
    info->library_version = "2019";
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
    
    if (!loadYZoomROM())
        return false;

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

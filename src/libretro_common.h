#if !defined(LIBRETRO_COMMON_H)
#define LIBRETRO_COMMON_H

#include <string>
#include <vector>

#include "bios.h"
#include "libretro.h"

class NeoGeoCD;

struct LibretroCallbacks
{
    retro_log_printf_t log{ nullptr };
    retro_video_refresh_t video{ nullptr };
    retro_input_poll_t inputPoll{ nullptr };
    retro_input_state_t inputState{ nullptr };
    retro_environment_t environment{ nullptr };
    retro_audio_sample_batch_t audioBatch{ nullptr };
    retro_perf_callback perf{ nullptr };
};

struct BiosListEntry
{
    std::string filename;
    std::string description;
    Bios::Type type;
};

struct Globals
{
    // Version number of the notification interface
    unsigned messageInterfaceVersion{ 0 };

    // Retroarch's system directory
    const char* systemDirectory{ nullptr };

    // Retroarch's save directory
    const char* saveDirectory{ nullptr };

    // Path to the srm file
    std::string srmFilename;

    // List of all BIOSes found
    std::vector<BiosListEntry> biosList;

    // Index of currently selected BIOS
    size_t biosIndex{ 0 };

    // Description of all BIOSes separated by |
    std::string biosChoices;

    // Should we skip CD loading?
    bool skipCDLoading{ true };

    // Should we patch the BIOS to lower CPU usage during loading
    bool cdSpeedHack{ false };

    bool perContentSaves{ false };
};

extern LibretroCallbacks libretro;

extern NeoGeoCD* neocd;

extern Globals globals;

#endif // LIBRETRO_COMMON_H

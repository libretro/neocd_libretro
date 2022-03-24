#include <streams/file_stream.h>
#include <file/file_path.h>
#include <retro_dirent.h>

#include "libretro_backupram.h"
#include "libretro_bios.h"
#include "libretro_common.h"
#include "libretro_input.h"
#include "libretro_log.h"
#include "libretro_memmap.h"
#include "libretro_variables.h"
#include "libretro.h"
#include "misc.h"
#include "neogeocd.h"

#ifdef VITA
#include <psp2/kernel/threadmgr.h>
#endif

void initVfs()
{
    retro_vfs_interface_info vfs_iface_info;
    vfs_iface_info.required_interface_version = DIRENT_REQUIRED_VFS_VERSION;
    vfs_iface_info.iface = nullptr;

    if (libretro.environment(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_iface_info))
    {
        Libretro::Log::message(RETRO_LOG_DEBUG, "Using front end provided VFS routines\n");

        filestream_vfs_init(&vfs_iface_info);
        path_vfs_init(&vfs_iface_info);
	dirent_vfs_init(&vfs_iface_info);
    }
    else
        Libretro::Log::message(RETRO_LOG_DEBUG, "Using fallback VFS routines\n");
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
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif
    info->library_version = "2022" GIT_VERSION;
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
        neocd->saveState(dummy);
        stateSize = dummy.size();
    }

    return stateSize;
}

bool retro_serialize(void *data, size_t size)
{
    if (size < retro_serialize_size())
        return false;

    DataPacker state(reinterpret_cast<char*>(data), 0, size);
    return neocd->saveState(state);
}

bool retro_unserialize(const void *data, size_t size)
{
    if (size < retro_serialize_size())
        return false;

    DataPacker state(const_cast<char*>(reinterpret_cast<const char*>(data)), size, size);

    if (neocd->restoreState(state))
        return true;

    Libretro::Bios::load();
    neocd->initialize();
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
    // Load the backup ram
    Libretro::BackupRam::setFilename(info->path);
    Libretro::BackupRam::load();

    // Initialize inputs
    Libretro::Input::init();

    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
    if (!libretro.environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
    {
        Libretro::Log::message(RETRO_LOG_ERROR, "RGB565 support is required!\n");
        return false;
    }

    // Load the BIOS
    if (!Libretro::Bios::load())
        return false;

    // Load the CD image
    if (!neocd->cdrom.loadCd(info->path))
        return false;

    // Update variables and reset
    Libretro::Variables::update(true);

    // Set libretro memory maps
    Libretro::Memmap::init();

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
    Libretro::BackupRam::save();
}

unsigned retro_get_region(void)
{
    return RETRO_REGION_NTSC;
}

void *retro_get_memory_data(unsigned id)
{
    if (id == RETRO_MEMORY_SYSTEM_RAM)
        return neocd->memory.ram;

    if (id == RETRO_MEMORY_VIDEO_RAM)
        return neocd->memory.videoRam;

    if (id == RETRO_MEMORY_SAVE_RAM)
        return neocd->memory.backupRam;

    return nullptr;
}

size_t retro_get_memory_size(unsigned id)
{
    if (id == RETRO_MEMORY_SYSTEM_RAM)
        return Memory::RAM_SIZE;

    if (id == RETRO_MEMORY_VIDEO_RAM)
        return Memory::VIDEORAM_SIZE;

    if (id == RETRO_MEMORY_SAVE_RAM)
        return Memory::BACKUPRAM_SIZE;

    return 0;
}

void retro_reset(void)
{
    neocd->reset();
}

void retro_run(void)
{
    // Update variables
    bool updated = false;
    if (libretro.environment(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
        Libretro::Variables::update(false);

    // Update inputs
    Libretro::Input::update();

    // Skip CD loading
    if (neocd->cdSectorDecodedThisFrame && globals.skipCDLoading)
    {
        neocd->fastForward = true;

        while (neocd->cdSectorDecodedThisFrame)
        {
            neocd->cdSectorDecodedThisFrame = false;
            neocd->runOneFrame();
        }

        neocd->fastForward = false;
    }

    // Run one frame
    neocd->cdSectorDecodedThisFrame = false;
    neocd->runOneFrame();

    // Send audio and video to the frontend
    libretro.audioBatch(reinterpret_cast<const int16_t*>(&neocd->audio.buffer.ymSamples[0]), neocd->audio.buffer.sampleCount);
    libretro.video(neocd->video.frameBuffer, Video::FRAMEBUFFER_WIDTH, Video::FRAMEBUFFER_HEIGHT, Video::FRAMEBUFFER_WIDTH * sizeof(uint16_t));
}

void retro_init(void)
{
    // Initialize the logging interface
    Libretro::Log::init();

    // Initialize VFS (If the call fails, fallback versions are used)
    initVfs();

    // Get the system directory
    libretro.environment(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &globals.systemDirectory);

    // Get the save directory
    libretro.environment(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &globals.saveDirectory);

    // Search for all supported BIOSes
    Libretro::Bios::init();

    // Initialize the emulation
    if (neocd == nullptr)
        neocd = new NeoGeoCD;

    // Initialize the CPU cores
    neocd->initialize();

    // Initialize variables
    Libretro::Variables::init();
}

void retro_deinit(void)
{
    Libretro::Log::message(RETRO_LOG_DEBUG, "NeoCD deinitializing\n");

    // Deinitialize the emulation
    if (neocd)
    {
        neocd->deinitialize();
        delete neocd;
        neocd = nullptr;
    }

#ifdef PROFILE_ENABLED
    Libretro::Log::message(RETRO_LOG_DEBUG, "Total frame time: %f ms\n", g_profilingAccumulators[ProfilingCategory::Total]);
    Libretro::Log::message(RETRO_LOG_DEBUG, " CD audio decoding: %f ms\n", g_profilingAccumulators[ProfilingCategory::AudioCD]);
    Libretro::Log::message(RETRO_LOG_DEBUG, " YM2610 generation and sound mixing: %f ms\n", g_profilingAccumulators[ProfilingCategory::AudioYM2610]);
    Libretro::Log::message(RETRO_LOG_DEBUG, " M68K emulation: %f ms\n", g_profilingAccumulators[ProfilingCategory::CpuM68K]);
    Libretro::Log::message(RETRO_LOG_DEBUG, " Z80 emulation: %f ms\n", g_profilingAccumulators[ProfilingCategory::CpuZ80]);
    Libretro::Log::message(RETRO_LOG_DEBUG, " Drawing video and handling IRQ: %f ms\n", g_profilingAccumulators[ProfilingCategory::VideoAndIRQ]);
    Libretro::Log::message(RETRO_LOG_DEBUG, "Time spent polling inputs: %f ms\n", g_profilingAccumulators[ProfilingCategory::InputPolling]);

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

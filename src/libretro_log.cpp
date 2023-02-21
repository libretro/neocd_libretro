#include <cstdio>
#include <cstdarg>
#include <stdio.h>

#include "libretro_common.h"
#include "libretro_log.h"

constexpr float INFO_DURATION_SECS = 1.0;
constexpr float WARN_DURATION_SECS = 5.0;
constexpr float ERROR_DURATION_SECS = 10.0;

constexpr int INFO_DURATION_MSECS = INFO_DURATION_SECS * 1000.0;
constexpr int INFO_DURATION_FRAMES = INFO_DURATION_SECS * 60.0;

constexpr int WARN_DURATION_MSECS = WARN_DURATION_SECS * 1000.0;
constexpr int WARN_DURATION_FRAMES = WARN_DURATION_SECS * 60.0;

constexpr int ERROR_DURATION_MSECS = ERROR_DURATION_SECS * 1000.0;
constexpr int ERROR_DURATION_FRAMES = ERROR_DURATION_SECS * 60.0;

constexpr int PRINTF_BUFFER_SIZE = 512;

void Libretro::Log::init()
{
    // Query the message interface version
    libretro.environment(RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION, &globals.messageInterfaceVersion);

    // Initialize the log interface
    struct retro_log_callback log;
    if (libretro.environment(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
        libretro.log = log.log;
    else
        libretro.log = nullptr;
}

void Libretro::Log::message(retro_log_level level, const char* format, ...)
{
    char buffer[PRINTF_BUFFER_SIZE];

    va_list args;
    va_start(args, format);
    int result = vsnprintf(buffer, PRINTF_BUFFER_SIZE, format, args);
    va_end(args);

    if (result < 0)
        return;

    if (libretro.log)
        libretro.log(level, buffer);

    if (level == RETRO_LOG_DEBUG)
        return;

    if (globals.messageInterfaceVersion >= 1)
    {
        struct retro_message_ext message = {
            buffer,
            1000,
            3,
            level,
            RETRO_MESSAGE_TARGET_OSD,
            RETRO_MESSAGE_TYPE_NOTIFICATION,
            -1
        };

        if (level == RETRO_LOG_ERROR)
        {
            message.duration = ERROR_DURATION_MSECS;
            message.priority = 3;
        }
        else if (level == RETRO_LOG_WARN)
        {
            message.duration = WARN_DURATION_MSECS;
            message.priority = 2;
        }
        else
        {
            message.duration = INFO_DURATION_MSECS;
            message.priority = 1;
        }

        libretro.environment(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &message);
    }
    else
    {
        struct retro_message message =
        {
            buffer,
            60
        };

          if (level == RETRO_LOG_ERROR)
            message.frames = ERROR_DURATION_FRAMES;
        else if (level == RETRO_LOG_WARN)
            message.frames = WARN_DURATION_FRAMES;
        else
            message.frames = INFO_DURATION_FRAMES;

        libretro.environment(RETRO_ENVIRONMENT_SET_MESSAGE, &message);
    }
}

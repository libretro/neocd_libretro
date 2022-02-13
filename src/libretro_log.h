#if !defined(LIBRETRO_LOG_H)
#define LIBRETRO_LOG_H

#include "libretro.h"

namespace Libretro
{
    namespace Log
    {
        void init();

        void message(retro_log_level level, const char* format, ...);
    } // namespace Log
} // namespace Libretro

#endif // LIBRETRO_LOG_H

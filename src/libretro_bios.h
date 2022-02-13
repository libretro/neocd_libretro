#if !defined(LIBRETRO_BIOS_H)
#define LIBRETRO_BIOS_H

#include <cstddef>

namespace Libretro
{
    namespace Bios
    {
        void init();

        bool load();

        size_t descriptionToIndex(const char* description);
    } // namespace Bios
} // namespace Libretro

#endif // LIBRETRO_BIOS_H

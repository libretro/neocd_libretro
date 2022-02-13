#if !defined(LIBRETRO_BACKUPRAM)
#define LIBRETRO_BACKUPRAM

namespace Libretro
{
    namespace BackupRam
    {
        void setFilename(const char* content_path);

        void load();

        void save();
    } // namespace BackupRam
} // namespace Libretro

#endif // LIBRETRO_BACKUPRAM

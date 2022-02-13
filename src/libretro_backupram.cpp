#include <cstring>
#include <streams/file_stream.h>

#include "libretro_backupram.h"
#include "libretro_common.h"
#include "libretro.h"
#include "memory.h"
#include "neogeocd.h"
#include "path.h"

void Libretro::BackupRam::setFilename(const char* content_path)
{
    globals.srmFilename = make_srm_path(globals.perContentSaves, content_path);
}

void Libretro::BackupRam::load()
{
    RFILE* file = filestream_open(globals.srmFilename.c_str(), RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
    if (!file)
        return;

    filestream_read(file, neocd->memory.backupRam, Memory::BACKUPRAM_SIZE);
    filestream_close(file);
}

void Libretro::BackupRam::save()
{
    RFILE* file = filestream_open(globals.srmFilename.c_str(), RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
    if (!file)
        return;

    filestream_write(file, neocd->memory.backupRam, Memory::BACKUPRAM_SIZE);
    filestream_close(file);
}

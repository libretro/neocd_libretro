#include "archive.h"
#include "archivezip.h"
#include "libretro_log.h"
#include "misc.h"
#include "path.h"

#include <cstring>

#include <file/archive_file.h>
#include <lists/string_list.h>

// libretro-common's archive backend does its own I/O through the VFS,
// so the file-callback shim this file used to carry for minizip is gone
// along with minizip itself.

namespace ArchiveZip
{
std::vector<std::string> getFileList(const std::string &archiveFilename)
{
    std::vector<std::string> result;

    struct string_list* list = file_archive_get_file_list(archiveFilename.c_str(), nullptr);
    if (!list)
    {
        Libretro::Log::message(RETRO_LOG_ERROR, "Archive: Could not open %s\n", archiveFilename.c_str());
        return result;
    }

    result.reserve(list->size);

    for (size_t i = 0; i < list->size; ++i)
    {
        const char* name = list->elems[i].data;
        if (!name || !*name)
            continue;

        // Directory entries carry a trailing separator; the old minizip
        // path filtered them on the MS-DOS directory attribute instead.
        size_t length = std::strlen(name);
        if ((name[length - 1] == '/') || (name[length - 1] == '\\'))
            continue;

        result.emplace_back(make_path_separator(archiveFilename.c_str(), "#", name));
    }

    string_list_free(list);

    return result;
}

int64_t getFileSize(const std::string &archive, const std::string &filename)
{
    std::string path = make_path_separator(archive.c_str(), "#", filename.c_str());

    uint64_t size = 0;

    // Reads the central directory only: the member is not decompressed.
    file_archive_get_file_crc32_and_size(path.c_str(), &size);
    if (!size)
    {
        Libretro::Log::message(RETRO_LOG_ERROR, "Archive: Could not find %s in archive %s\n", filename.c_str(), archive.c_str());
        return -1;
    }

    return static_cast<int64_t>(size);
}

bool readFile(const std::string &archive, const std::string &filename, void *buffer, size_t maximumSize, size_t *reallyRead)
{
    std::string path = make_path_separator(archive.c_str(), "#", filename.c_str());

    void* data = nullptr;
    int64_t length = 0;

    if (!file_archive_compressed_read(path.c_str(), &data, nullptr, &length) || !data)
    {
        Libretro::Log::message(RETRO_LOG_ERROR, "Archive: Could not read %s in archive %s\n", filename.c_str(), archive.c_str());
        if (data)
            free(data);
        return false;
    }

    // The backend allocates the whole member; the caller takes what fits.
    size_t copied = static_cast<size_t>(length);
    if (copied > maximumSize)
        copied = maximumSize;

    std::memcpy(buffer, data, copied);
    free(data);

    if (reallyRead)
        *reallyRead = copied;

    return true;
}

} // namespace ArchiveZip

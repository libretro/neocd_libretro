#include "archive.h"
#include "archivezip.h"
#include "misc.h"
#include "path.h"
#include "unzip.h"
#include "streams/file_stream.h"

static voidpf ZCALLBACK fopen_file_func(voidpf opaque, const char* filename, int mode)
{
    UNUSED_ARG(opaque);

    RFILE* file = nullptr;

    unsigned mode_fopen = 0;

    if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER) == ZLIB_FILEFUNC_MODE_READ)
        mode_fopen = RETRO_VFS_FILE_ACCESS_READ;
    else if (mode & ZLIB_FILEFUNC_MODE_EXISTING)
        mode_fopen = RETRO_VFS_FILE_ACCESS_READ_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING;
    else if (mode & ZLIB_FILEFUNC_MODE_CREATE)
        mode_fopen = RETRO_VFS_FILE_ACCESS_WRITE;

    if ((filename != nullptr) && (mode_fopen != 0))
        file = filestream_open(filename, mode_fopen, RETRO_VFS_FILE_ACCESS_HINT_NONE);

    return file;
}

static int64_t ZCALLBACK fread_file_func(voidpf opaque, voidpf stream, void* buf, uLong size)
{
    UNUSED_ARG(opaque);

    return filestream_read(reinterpret_cast<RFILE*>(stream), buf, int64_t(size));
}

static int64_t ZCALLBACK fwrite_file_func(voidpf opaque, voidpf stream, const void* buf, uLong size)
{
    UNUSED_ARG(opaque);

    return filestream_write(reinterpret_cast<RFILE*>(stream), buf, int64_t(size));
}

static int64_t ZCALLBACK ftell_file_func(voidpf opaque, voidpf stream)
{
    UNUSED_ARG(opaque);

    return filestream_tell(reinterpret_cast<RFILE*>(stream));
}

static int64_t ZCALLBACK fseek_file_func(voidpf  opaque, voidpf stream, uLong offset, int origin)
{
    int64_t ret = 0;
    int fseek_origin = 0;

    UNUSED_ARG(opaque);

    switch (origin)
    {
    case ZLIB_FILEFUNC_SEEK_CUR :
        fseek_origin = RETRO_VFS_SEEK_POSITION_CURRENT;
        break;
    case ZLIB_FILEFUNC_SEEK_END :
        fseek_origin = RETRO_VFS_SEEK_POSITION_END;
        break;
    case ZLIB_FILEFUNC_SEEK_SET :
        fseek_origin = RETRO_VFS_SEEK_POSITION_START;
        break;
    default: return -1;
    }

    if (filestream_seek(reinterpret_cast<RFILE*>(stream), offset, fseek_origin) != 0)
        return -1;

    return ret;
}

static int ZCALLBACK fclose_file_func(voidpf opaque, voidpf stream)
{
    UNUSED_ARG(opaque);

    return filestream_close(reinterpret_cast<RFILE*>(stream));
}

static int ZCALLBACK ferror_file_func(voidpf opaque, voidpf stream)
{
    UNUSED_ARG(opaque);

    return filestream_error(reinterpret_cast<RFILE*>(stream));
}

static void fill_callbacks(zlib_filefunc_def* callbacks)
{
    callbacks->zopen_file = fopen_file_func;
    callbacks->zread_file = fread_file_func;
    callbacks->zwrite_file = fwrite_file_func;
    callbacks->ztell_file = ftell_file_func;
    callbacks->zseek_file = fseek_file_func;
    callbacks->zclose_file = fclose_file_func;
    callbacks->zerror_file = ferror_file_func;
    callbacks->opaque = NULL;
}

namespace ArchiveZip
{
std::vector<std::string> getFileList(const std::string &archiveFilename)
{
    std::vector<std::string> result;

    zlib_filefunc_def callbacks;
    fill_callbacks(&callbacks);

    auto zipFile = unzOpen2(archiveFilename.c_str(), &callbacks);
    if (!zipFile)
    {
        LOG(LOG_ERROR, "Archive: Could not open %s\n", archiveFilename.c_str());
        return result;
    }

    while(1)
    {
        unz_file_info fileInfo;

        if (unzGetCurrentFileInfo(zipFile, &fileInfo, nullptr, 0, nullptr, 0, nullptr, 0) != UNZ_OK)
        {
            LOG(LOG_ERROR, "Archive: Failed to enumerate files (1) %s\n", archiveFilename.c_str());
            break;
        }

        if (!(fileInfo.external_fa & 16))
        {
            std::string filename(fileInfo.size_filename, 0x0);
            if (unzGetCurrentFileInfo(zipFile, &fileInfo, &filename[0], filename.size(), nullptr, 0, nullptr, 0) != UNZ_OK)
            {
                LOG(LOG_ERROR, "Archive: Failed to enumerate files (2) %s\n", archiveFilename.c_str());
                break;
            }

            result.emplace_back(make_path_separator(archiveFilename.c_str(), "#", filename.c_str()));
        }

        if (unzGoToNextFile(zipFile) != UNZ_OK)
            break;
    }

    if (unzClose(zipFile) != UNZ_OK)
        LOG(LOG_ERROR, "Archive: Could not close %s\n", archiveFilename.c_str());

    return result;
}

bool readFile(const std::string &archive, const std::string &filename, void *buffer, size_t maximumSize, size_t *reallyRead)
{
    zlib_filefunc_def callbacks;
    fill_callbacks(&callbacks);

    auto zipFile = unzOpen2(archive.c_str(), &callbacks);
    if (!zipFile)
    {
        LOG(LOG_ERROR, "Archive: Could not open %s\n", archive.c_str());
        return false;
    }

    auto cleanup = [&](bool result) -> bool
    {
        if (unzClose(zipFile) != UNZ_OK)
            LOG(LOG_ERROR, "Archive: Could not close %s\n", archive.c_str());

        return result;
    };

    if (unzLocateFile(zipFile, filename.c_str(), 2) != UNZ_OK)
    {
        LOG(LOG_ERROR, "Archive: Could not find %s in archive %s\n", filename.c_str(), archive.c_str());
        return cleanup(false);
    }

    if (unzOpenCurrentFile(zipFile) != UNZ_OK)
    {
        LOG(LOG_ERROR, "Archive: Could not open %s in archive %s\n", filename.c_str(), archive.c_str());
        return cleanup(false);
    }

    auto result = unzReadCurrentFile(zipFile, buffer, maximumSize);
    if (result < 0)
    {
        LOG(LOG_ERROR, "Archive: Could not read %s in archive %s\n", filename.c_str(), archive.c_str());
        unzCloseCurrentFile(zipFile);
        return cleanup(false);
    }

    if (reallyRead)
        *reallyRead = result;

    if (unzCloseCurrentFile(zipFile) != UNZ_OK)
        LOG(LOG_ERROR, "Archive: Could not close %s in archive %s\n", filename.c_str(), archive.c_str());

    return cleanup(true);
}

} // namespace ArchiveZip

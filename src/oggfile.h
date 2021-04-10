#ifndef OGGFILE_H
#define OGGFILE_H

#define OV_EXCLUDE_STATIC_CALLBACKS

#include <cstdint>

#include "abstractfile.h"
#include "vorbis/vorbisfile.h"

class OggFile
{
public:
    OggFile();
    ~OggFile();

    // Non copyable
    OggFile(const OggFile&) = delete;

    // Non copyable
    OggFile& operator=(const OggFile&) = delete;

    bool initialize(AbstractFile* file);

    size_t read(char *data, size_t size);

    bool seek(size_t position);

    size_t length();

    void cleanup();

    // Declare all callbacks as friends so they can modify the internals
    friend size_t ogg_read_cb(void *ptr, size_t size, size_t nmemb, void *datasource);
    friend int ogg_seek_cb(void *datasource, ogg_int64_t offset, int whence);
    friend long ogg_tell_cb(void *datasource);

protected:
    OggVorbis_File m_vorbisFile;
    AbstractFile* m_file;
    bool m_isOpen;
};

#endif // OGGFILE_H

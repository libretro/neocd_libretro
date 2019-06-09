#include "oggfile.h"

// Ogg read callback
size_t ogg_read_cb(void *ptr, size_t size, size_t nmemb, void *datasource)
{
    OggFile* oggFile = reinterpret_cast<OggFile*>(datasource);
    AbstractFile* file = oggFile->m_file;
    if ((!file) || (!file->isOpen()))
        return 0;

    return file->readData(ptr, size * nmemb);
}

// Ogg seek callback
int ogg_seek_cb(void *datasource, ogg_int64_t offset, int whence)
{
    OggFile* oggFile = reinterpret_cast<OggFile*>(datasource);
    AbstractFile* file = oggFile->m_file;
    if ((!file) || (!file->isOpen()))
        return -1;

    bool success = false;

    switch (whence)
    {
    case SEEK_CUR:
        success = file->seek(static_cast<size_t>(file->pos() + offset));
        break;
    case SEEK_END:
        success = file->seek(file->size() + static_cast<size_t>(offset));
        break;
    default:
        success = file->seek(static_cast<size_t>(offset));
        break;
    }

    if (!success)
        return -1;

    return 0;
}

// Ogg tell callback
long ogg_tell_cb(void *datasource)
{
    OggFile* oggFile = reinterpret_cast<OggFile*>(datasource);
    AbstractFile* file = oggFile->m_file;
    if ((!file) || (!file->isOpen()))
        return -1;

    return static_cast<long>(file->pos());
}

static const ov_callbacks ogg_callbacks = {
    ogg_read_cb,
    ogg_seek_cb,
    nullptr,
    ogg_tell_cb
};

OggFile::OggFile() :
    m_vorbisFile(),
    m_file(nullptr),
    m_isOpen(false)
{
}

OggFile::~OggFile()
{
    cleanup();
}

bool OggFile::initialize(AbstractFile* file)
{
    cleanup();

    m_file = file;

    if (ov_open_callbacks(this, &m_vorbisFile, nullptr, 0, ogg_callbacks) != 0)
        return false;

    m_isOpen = true;
    return true;
}

size_t OggFile::read(char *data, size_t size)
{
    if (!m_isOpen)
        return 0;

    size_t done = 0;
    size_t result;
    int bitstream;

    while (size)
    {
        result = static_cast<size_t>(ov_read(&m_vorbisFile, data, static_cast<int>(size), 0, 2, 1, &bitstream));
        if (result <= 0)
            return done;

        data += result;
        done += result;
        size -= result;
    }

    return done;
}

bool OggFile::seek(size_t position)
{
    if (!m_isOpen)
        return false;

    return (ov_pcm_seek(&m_vorbisFile, position / 4) == 0);
}

size_t OggFile::length()
{
    if (!m_isOpen)
        return 0;

    return (static_cast<size_t>(ov_pcm_total(&m_vorbisFile, -1) * 4));
}

void OggFile::cleanup()
{
    if (m_isOpen)
    {
        ov_clear(&m_vorbisFile);
        m_file = nullptr;
        m_isOpen = false;
    }
}

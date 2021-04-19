#include <cstring>

//#define DR_FLAC_IMPLEMENTATION
#include "flacfile.h"

size_t drflac_read_cb(void* pUserData, void* pBufferOut, size_t bytesToRead)
{
    auto file = reinterpret_cast<AbstractFile*>(pUserData);
    return file->readData(pBufferOut, bytesToRead);
}

drflac_bool32 drflac_seek_cb(void* pUserData, int offset, drflac_seek_origin origin)
{
    auto file = reinterpret_cast<AbstractFile*>(pUserData);

    int64_t destination;

    if (origin == drflac_seek_origin_current)
        destination = file->pos() + offset;
    else
        destination = offset;

    if (!file->seek(destination))
        return DRFLAC_FALSE;

    return DRFLAC_TRUE;
}

FlacFile::FlacFile() :
    m_file(nullptr),
    m_flac(nullptr),
    m_remainder(),
    m_remainderPos(4)
{
}

FlacFile::~FlacFile()
{
    cleanup();
}

bool FlacFile::initialize(AbstractFile *file)
{
    cleanup();

    m_file = file;

    if (!m_file->isOpen())
        return false;

    m_flac = drflac_open(drflac_read_cb, drflac_seek_cb, m_file, nullptr);
    if (m_flac == nullptr)
        return false;

    if ((m_flac->channels != 2) || (m_flac->sampleRate != 44100))
    {
        cleanup();
        return false;
    }

    return true;
}

size_t FlacFile::read(char *data, size_t size)
{
    size_t result = 0;

    auto readBytes = readRemainder(data, size);
    data += readBytes;
    size -= readBytes;
    result += readBytes;

    if (size == 0)
        return result;

    auto readFrames = drflac_read_pcm_frames_s16(m_flac, size / 4, reinterpret_cast<drflac_int16*>(data));
    data += readFrames * 4;
    size -= readFrames * 4;
    result += readFrames * 4;

    if (size == 0)
        return result;

    readFrames = drflac_read_pcm_frames_s16(m_flac, 1, reinterpret_cast<drflac_int16*>(&m_remainder[0]));
    if (readFrames != 0)
        m_remainderPos = 0;

    readBytes = readRemainder(data, size);
    data += readBytes;
    size -= readBytes;
    result += readBytes;

    return result;
}

bool FlacFile::seek(size_t position)
{
    if (m_flac == nullptr)
        return false;

    clearRemainder();

    return (drflac_seek_to_pcm_frame(m_flac, position / 4) == DRFLAC_TRUE);
}

size_t FlacFile::length()
{
    if (m_flac == nullptr)
        return 0;

    return m_flac->totalPCMFrameCount * 4;
}

void FlacFile::cleanup()
{
    if (m_flac != nullptr)
        drflac_close(m_flac);

    m_flac = nullptr;
    m_file = nullptr;
    clearRemainder();
}

size_t FlacFile::readRemainder(char *data, size_t size)
{
    const size_t available = 4 - m_remainderPos;

    if (size > available)
        size = available;

    std::memcpy(data, &m_remainder[m_remainderPos], size);
    m_remainderPos += size;

    return size;
}

void FlacFile::clearRemainder()
{
    m_remainderPos = 4;
}

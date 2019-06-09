#include <algorithm>
#include <cassert>

#include "endian.h"
#include "wavfile.h"
#include "wavstruct.h"

WavFile::WavFile() :
    m_file(nullptr),
    m_currentPosition(0),
    m_dataStart(0),
    m_dataSize(0)
{
}

WavFile::~WavFile()
{
}

bool WavFile::initialize(AbstractFile *file)
{
    cleanup();

    m_file = file;

    if (!m_file->isOpen())
        return false;

    WaveRiffHeader waveHeader;
    WaveChunkHeader chunkHeader;
    WaveFmtChunk fmtHeader;

    // First read in the header
    if (file->readData(&waveHeader, sizeof(waveHeader)) < static_cast<int64_t>(sizeof(waveHeader)))
        return false;

    // ... and check that indeed it is a WAVE file
    if ((waveHeader.magic != LITTLE_ENDIAN_DWORD(0x46464952)) || (waveHeader.formatId != LITTLE_ENDIAN_DWORD(0x45564157)))
        return false;

    size_t fileSize = LITTLE_ENDIAN_DWORD(waveHeader.fileSize) + 8;
    int64_t fmtHeaderPos = 0;
    int64_t dataPos = 0;
    int64_t dataSize = 0;

    // Next, scan all chunks in the WAVE file looking for two specific chunks: fmt and data
    do
    {
        if (file->readData(&chunkHeader, sizeof(chunkHeader)) < static_cast<int64_t>(sizeof(chunkHeader)))
            return false;

        int64_t currentPosition = file->pos();
        if (currentPosition < 0)
            return false;

        if (static_cast<size_t>(currentPosition) + LITTLE_ENDIAN_DWORD(chunkHeader.dataSize) > fileSize)
            return false;

        if (chunkHeader.magic == LITTLE_ENDIAN_DWORD(0x20746d66))
            fmtHeaderPos = currentPosition;
        else if (chunkHeader.magic == LITTLE_ENDIAN_DWORD(0x61746164))
        {
            dataPos = currentPosition;
            dataSize = LITTLE_ENDIAN_DWORD(chunkHeader.dataSize);
        }

        file->skip(LITTLE_ENDIAN_DWORD(chunkHeader.dataSize));
    } while((dataPos == 0) || (fmtHeaderPos == 0));

    // Read in the fmt chunk
    file->seek(static_cast<size_t>(fmtHeaderPos));
    if (file->readData(&fmtHeader, sizeof(fmtHeader)) < static_cast<int64_t>(sizeof(fmtHeader)))
        return false;

    // ... and check that the audio has the desired format
    if ((fmtHeader.audioFormat != LITTLE_ENDIAN_WORD(1))
            || (fmtHeader.bitsPerSample != LITTLE_ENDIAN_WORD(16))
            || (fmtHeader.sampleRate != LITTLE_ENDIAN_DWORD(44100)))
        return false;

    // All ok!
    m_currentPosition = 0;
    m_dataStart = dataPos;
    m_dataSize = dataSize;

    file->seek(static_cast<size_t>(m_dataStart));

    return true;
}

int64_t WavFile::read(void *data, int64_t size)
{
    if ((!m_file) || (m_dataSize <= 0))
        return 0;

    int64_t available = m_dataSize - m_currentPosition;
    int64_t slice = std::min(size, available);

    int64_t done = static_cast<int64_t>(m_file->readData(data, static_cast<size_t>(slice)));

    m_currentPosition += done;

    return done;
}

bool WavFile::seek(int64_t position)
{
    m_currentPosition = std::min(position, m_dataSize);

    m_file->seek(static_cast<size_t>(m_currentPosition + m_dataStart));

    return true;
}

int64_t WavFile::length()
{
    if ((!m_file) || (m_dataSize <= 0))
        return 0;

    return m_dataSize;
}

void WavFile::cleanup()
{
    m_currentPosition = 0;
    m_dataStart = 0;
    m_dataSize = 0;
}

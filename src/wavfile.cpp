#include "wavfile.h"
#include "packedstruct.h"
#include "endian.h"

#include <algorithm>
#include <stdint.h>
#include <cassert>

#ifdef _MSC_VER
    #pragma pack(push,1)
#endif

typedef struct PACKED {
    uint32_t magic;
    uint32_t fileSize;
    uint32_t formatId;
} wave_riff_header_t;

typedef struct PACKED {
    uint16_t audioFormat;
    uint16_t channelCount;
    uint32_t sampleRate;
    uint32_t bytesPerSecond;
    uint16_t bytesPerBlock;
    uint16_t bitsPerSample;
} wave_fmt_header_t;

typedef struct PACKED {
    uint32_t magic;
    uint32_t dataSize;
} wave_chunk_header_t;

#ifdef _MSC_VER
    #pragma pack(pop)
#endif

WavFile::WavFile() :
    m_file(nullptr),
    m_currentPosition(0),
    m_dataStart(0),
    m_dataSize(0)
{
    assert(sizeof(wave_riff_header_t) == 12);
    assert(sizeof(wave_fmt_header_t) == 16);
    assert(sizeof(wave_chunk_header_t) == 8);
}

WavFile::~WavFile()
{
}

bool WavFile::initialize(std::ifstream *file)
{
    cleanup();

    m_file = file;

    if (!m_file->is_open())
        return false;

    wave_riff_header_t waveHeader;
    wave_chunk_header_t chunkHeader;
    wave_fmt_header_t fmtHeader;

    // First read in the header
    file->read(reinterpret_cast<char*>(&waveHeader), sizeof(waveHeader));
    if (file->gcount() < sizeof(waveHeader))
        return false;

    // ... and check that indeed it is a WAVE file
    if ((waveHeader.magic != LITTLE_ENDIAN_DWORD(0x46464952)) || (waveHeader.formatId != LITTLE_ENDIAN_DWORD(0x45564157)))
        return false;

    size_t fileSize = LITTLE_ENDIAN_DWORD(waveHeader.fileSize) + 8;
    size_t fmtHeaderPos = 0;
    size_t dataPos = 0;
    size_t dataSize = 0;

    // Next, scan all chunks in the WAVE file looking for two specific chunks: fmt and data
    do
    {
        file->read(reinterpret_cast<char*>(&chunkHeader), sizeof(chunkHeader));
        if (file->gcount() < sizeof(chunkHeader))
            return false;

        std::ios::pos_type currentPosition = file->tellg();
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

        file->seekg(LITTLE_ENDIAN_DWORD(chunkHeader.dataSize), std::ios::cur);
    } while((dataPos == 0) || (fmtHeaderPos == 0));

    // Read in the fmt chunk
    file->seekg(fmtHeaderPos, std::ios::beg);
    file->read(reinterpret_cast<char*>(&fmtHeader), sizeof(fmtHeader));
    if (file->gcount() < sizeof(fmtHeader))
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

    file->seekg(m_dataStart, std::ios::beg);

    return true;
}

size_t WavFile::read(char *data, size_t size)
{
    if ((!m_file) || (m_dataSize <= 0))
        return 0;

    size_t available = m_dataSize - m_currentPosition;
    size_t slice = std::min(size, available);

    m_file->read(data, slice);

	// Fix ifstream stupidity: eof is not a failure condition
	if (m_file->fail() && m_file->eof())
		m_file->clear();

    size_t done = static_cast<size_t>(m_file->gcount());
    m_currentPosition += done;

    return done;
}

bool WavFile::seek(size_t position)
{
    m_currentPosition = std::min(position, m_dataSize);

    m_file->seekg(m_currentPosition + m_dataStart, std::ios::beg);

    return true;
}

size_t WavFile::length()
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

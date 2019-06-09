#ifndef WAVSTRUCT_H
#define WAVSTRUCT_H

#include <cstdint>

#include "packedstruct.h"

#ifdef _MSC_VER
    #pragma pack(push,1)
#endif

struct PACKED WaveRiffHeader
{
    uint32_t magic;
    uint32_t fileSize;
    uint32_t formatId;
};

static_assert(sizeof(WaveRiffHeader) == 12, "Struct RIFF Header should be exactly 12 bytes!");

struct PACKED WaveFmtChunk
{
    uint16_t audioFormat;
    uint16_t channelCount;
    uint32_t sampleRate;
    uint32_t bytesPerSecond;
    uint16_t bytesPerBlock;
    uint16_t bitsPerSample;
};

static_assert(sizeof(WaveFmtChunk) == 16, "Struct Wave Format Header should be exactly 16 bytes!");

struct PACKED WaveChunkHeader
{
    uint32_t magic;
    uint32_t dataSize;
};

static_assert(sizeof(WaveChunkHeader) == 8, "Struct Wave Chunk Header should be exactly 8 bytes!");

#ifdef _MSC_VER
    #pragma pack(pop)
#endif

#endif // WAVSTRUCT_H

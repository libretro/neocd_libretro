#ifndef WAVFILE_H
#define WAVFILE_H

#include <cstdint>
#include <cstddef>
#include <vector>

#include "abstractfile.h"

#include <formats/rwav.h>

class WavFile
{
public:
    WavFile();
    ~WavFile();

    // Non copyable
    WavFile(const WavFile&) = delete;

    // Non copyable
    WavFile& operator=(const WavFile&) = delete;

    bool initialize(AbstractFile* file);

    int64_t read(void *data, int64_t size);

    bool seek(int64_t position);

    int64_t length();

    void cleanup();

protected:
    // Reads the RIFF chunk list, growing the resident head until the
    // 'data' chunk header is inside it. The payload is never resident.
    bool parseHeader();

    AbstractFile* m_file;
    rwav_t m_wav;
    std::vector<uint8_t> m_chunk;   // scratch for one read's byte extent
    int64_t m_currentFrame;
    int64_t m_totalFrames;
    bool m_isOpen;
};

#endif // WAVFILE_H

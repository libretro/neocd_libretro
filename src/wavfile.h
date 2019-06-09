#ifndef WAVFILE_H
#define WAVFILE_H

#include <cstdint>

#include "abstractfile.h"

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
    AbstractFile* m_file;
    int64_t m_currentPosition;
    int64_t m_dataStart;
    int64_t m_dataSize;
};

#endif // WAVFILE_H

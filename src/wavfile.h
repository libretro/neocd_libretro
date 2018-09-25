#ifndef WAVFILE_H
#define WAVFILE_H

#include <fstream>

class WavFile
{
public:
    WavFile();
    ~WavFile();

    // Non copyable
    WavFile(const WavFile&) = delete;

    // Non copyable
    WavFile& operator=(const WavFile&) = delete;

    bool initialize(std::ifstream* file);

    size_t read(char *data, size_t size);

    bool seek(size_t position);

    size_t length();

    void cleanup();

protected:
    std::ifstream* m_file;
    size_t m_currentPosition;
    size_t m_dataStart;
    size_t m_dataSize;
};

#endif // WAVFILE_H

#ifndef FLACFILE_H
#define FLACFILE_H

#include <array>

#include "abstractfile.h"
#include "dr_libs/dr_flac.h"

class FlacFile
{
public:
    FlacFile();
    ~FlacFile();

    // Non copyable
    FlacFile(const FlacFile&) = delete;

    // Non copyable
    FlacFile& operator=(const FlacFile&) = delete;

    bool initialize(AbstractFile* file);

    size_t read(char *data, size_t size);

    bool seek(size_t position);

    size_t length();

    void cleanup();

    // Declare all callbacks as friends so they can modify the internals
    friend size_t drflac_read_cb(void* pUserData, void* pBufferOut, size_t bytesToRead);
    friend drflac_bool32 drflac_seek_cb(void* pUserData, int offset, drflac_seek_origin origin);

protected:
    size_t readRemainder(char *data, size_t size);
    void clearRemainder();

    AbstractFile* m_file;
    drflac* m_flac;
    std::array<char, 4> m_remainder;
    int m_remainderPos;
};

#endif // FLACFILE_H

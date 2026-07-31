#ifndef FLACFILE_H
#define FLACFILE_H

#include <cstdint>
#include <cstddef>
#include <vector>

#include "abstractfile.h"

#include <formats/rflac.h>

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

protected:
    bool refill();

    // Re-points input at a byte offset and tells the decoder where it
    // resumed, so it can resynchronise and report position from there.
    bool resumeAt(uint64_t byteOffset, uint64_t frame);

    // Decodes forward from the current position, discarding, until the
    // target frame. Used when the stream carries no seek table, which
    // is the common case for a ripped track.
    bool decodeForwardTo(uint64_t frame);

    // A frame must be presented whole, and how large one can be is not
    // known until the header is parsed, so the window is sized from
    // rflac_min_input() rather than guessed at.
    static constexpr size_t WINDOW_INITIAL = 32768;

    AbstractFile* m_file;
    rflac_t* m_flac;
    std::vector<uint8_t> m_window;
    size_t m_filled;
    uint64_t m_fileOffset;   // where the unconsumed window begins
    uint64_t m_totalFrames;
    bool m_isOpen;
};

#endif // FLACFILE_H

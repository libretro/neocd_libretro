#ifndef MP3FILE_H
#define MP3FILE_H

#include <cstdint>
#include <cstddef>
#include <vector>

#include "abstractfile.h"

#include <formats/rmp3.h>

class Mp3File
{
public:
    Mp3File();
    ~Mp3File();

    // Non copyable
    Mp3File(const Mp3File&) = delete;

    // Non copyable
    Mp3File& operator=(const Mp3File&) = delete;

    bool initialize(AbstractFile* file);

    size_t read(char *data, size_t size);

    bool seek(size_t position);

    size_t length();

    void cleanup();

protected:
    bool refill();

    // Walks every frame without decoding one, which is the only way to
    // learn a stream's length: MPEG audio states none, and a Xing header
    // is a convention rips frequently omit. Records where frames begin
    // as it goes, since the walk is the expensive part and doing it
    // again to seek would cost the same.
    bool scan();

    // Re-points input at a frame boundary and decodes forward to an
    // exact position, discarding. A frame resumed mid-stream is missing
    // whatever the bit reservoir carried into it, so the landing is
    // taken far enough back for that to have washed out.
    bool seekToFrame(uint64_t frame);

    static constexpr size_t WINDOW_SIZE = 16384;

    // One index entry per this many MPEG frames. At 1152 samples each,
    // 64 frames is about 1.7s: enough to keep the index small on a long
    // track while bounding how far a seek has to decode.
    static constexpr uint32_t INDEX_INTERVAL = 64;

    // Index entries are placed this far back from a seek target, so
    // there is stream ahead of it to establish the bit reservoir with,
    // and the frames that decode wrong for want of one are behind the
    // target rather than at it.
    static constexpr uint32_t RESERVOIR_WARMUP = 2;

    AbstractFile* m_file;
    rmp3_stream_t* m_stream;
    uint8_t m_window[WINDOW_SIZE];
    size_t m_filled;
    uint64_t m_fileOffset;
    uint64_t m_totalFrames;
    uint64_t m_position;
    std::vector<uint64_t> m_index;   // byte offset every INDEX_INTERVAL frames
    bool m_eofSent;
    bool m_isOpen;
};

#endif // MP3FILE_H

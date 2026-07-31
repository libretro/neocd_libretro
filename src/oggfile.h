#ifndef OGGFILE_H
#define OGGFILE_H

#include <cstdint>
#include <cstddef>

#include "abstractfile.h"

#include <formats/rvorbis.h>

class OggFile
{
public:
    OggFile();
    ~OggFile();

    // Non copyable
    OggFile(const OggFile&) = delete;

    // Non copyable
    OggFile& operator=(const OggFile&) = delete;

    bool initialize(AbstractFile* file);

    size_t read(char *data, size_t size);

    bool seek(size_t position);

    size_t length();

    void cleanup();

protected:
    // Tops the input window up from the file. Returns false at EOF with
    // nothing added.
    bool refill();

    // Resynchronises at a byte offset and decodes forward to an exact
    // frame. Ogg states positions only at page boundaries, so landing
    // exactly means decoding the frames in between and dropping them.
    bool seekToFrame(uint64_t frame);

    // The stream's last page granule, which is its length in frames.
    // Read from the tail at open: a granule is only stated on a page,
    // and the last one is the only page that states the total.
    bool readTotalFrames();

    // One maximum Ogg page. The demuxer accumulates packets itself, so
    // this need not hold a whole page - it only bounds how often the
    // file is touched.
    static constexpr size_t WINDOW_SIZE = 16384;

    AbstractFile* m_file;
    rvorbis_stream_t* m_stream;
    uint8_t m_window[WINDOW_SIZE];
    size_t m_filled;
    size_t m_fileOffset;   // where the unconsumed window begins
    uint64_t m_totalFrames;
    bool m_isOpen;
};

#endif // OGGFILE_H

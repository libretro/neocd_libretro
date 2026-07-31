#include <cstring>
#include <vector>

#include "libretro_log.h"
#include "oggfile.h"

// CD audio is 44.1kHz stereo by definition, and the track is played
// through a pipeline built around that, so a stream that is not gets
// refused rather than played at the wrong speed.
constexpr unsigned CD_AUDIO_RATE = 44100;
constexpr unsigned CD_AUDIO_CHANNELS = 2;
constexpr size_t BYTES_PER_FRAME = 4;

OggFile::OggFile() :
    m_file(nullptr),
    m_stream(nullptr),
    m_window(),
    m_filled(0),
    m_fileOffset(0),
    m_totalFrames(0),
    m_isOpen(false)
{
}

OggFile::~OggFile()
{
    cleanup();
}

bool OggFile::refill()
{
    if (m_filled >= WINDOW_SIZE)
        return true;

    size_t got = m_file->readData(m_window + m_filled, WINDOW_SIZE - m_filled);
    m_filled += got;

    return (got != 0);
}

bool OggFile::readTotalFrames()
{
    // The final page is within one maximum page of the end.
    constexpr size_t MAX_PAGE = 65307;

    size_t fileSize = m_file->size();
    size_t tail = (fileSize > MAX_PAGE) ? MAX_PAGE : fileSize;
    size_t base = fileSize - tail;

    std::vector<uint8_t> buffer(tail);

    if (!m_file->seek(base))
        return false;
    if (m_file->readData(buffer.data(), tail) != tail)
        return false;

    // Walk back to the last capture pattern that states a position; a
    // page on which no packet completes carries -1 instead. The scan
    // starts far enough from the end that a whole page header fits.
    constexpr size_t PAGE_HEADER = 27;

    if (tail < PAGE_HEADER)
        return false;

    for (size_t i = tail - PAGE_HEADER + 1; i-- > 0; )
    {
        if (std::memcmp(buffer.data() + i, "OggS", 4) != 0)
            continue;

        uint64_t granule = 0;
        for (unsigned k = 0; k < 8; ++k)
            granule |= static_cast<uint64_t>(buffer[i + 6 + k]) << (8 * k);

        if (granule == static_cast<uint64_t>(-1))
            continue;

        m_totalFrames = granule;
        return true;
    }

    return false;
}

bool OggFile::initialize(AbstractFile *file)
{
    cleanup();

    m_file = file;

    if ((!m_file) || (!m_file->isOpen()))
        return false;

    if (!readTotalFrames())
    {
        Libretro::Log::message(RETRO_LOG_ERROR, "Ogg: could not determine stream length.\n");
        return false;
    }

    if (!m_file->seek(0))
        return false;

    m_stream = rvorbis_stream_new();
    if (!m_stream)
        return false;

    m_filled = 0;
    m_fileOffset = 0;

    // Feed until the setup headers are in. A null output means
    // parse-only, so nothing is decoded before the format is known.
    rvorbis_info info;
    for (;;)
    {
        size_t consumed = 0;
        size_t produced = 0;

        if (!refill() && !m_filled)
        {
            cleanup();
            return false;
        }

        rvorbis_stream_set_out_s16(m_stream, nullptr, 0);
        rvorbis_stream_set_in(m_stream, m_window, m_filled);

        int status = rvorbis_stream_process(m_stream, &consumed, &produced);

        std::memmove(m_window, m_window + consumed, m_filled - consumed);
        m_filled -= consumed;
        m_fileOffset += consumed;

        if (rvorbis_stream_info(m_stream, &info))
            break;

        if (status == RVORBIS_STREAM_ERROR || status == RVORBIS_STREAM_EOS)
        {
            cleanup();
            return false;
        }
    }

    if ((info.channels != static_cast<int>(CD_AUDIO_CHANNELS)) || (info.sample_rate != CD_AUDIO_RATE))
    {
        Libretro::Log::message(RETRO_LOG_ERROR,
            "Ogg: %u channels at %uHz is not CD audio (expected %u at %uHz).\n",
            info.channels, info.sample_rate, CD_AUDIO_CHANNELS, CD_AUDIO_RATE);
        cleanup();
        return false;
    }

    m_isOpen = true;
    return true;
}

size_t OggFile::read(char *data, size_t size)
{
    if (!m_isOpen)
        return 0;

    size_t frames = size / BYTES_PER_FRAME;
    size_t done = 0;

    while (done < frames)
    {
        size_t consumed = 0;
        size_t produced = 0;

        rvorbis_stream_set_out_s16(m_stream,
            reinterpret_cast<int16_t*>(data) + done * CD_AUDIO_CHANNELS,
            frames - done);
        rvorbis_stream_set_in(m_stream, m_window, m_filled);

        int status = rvorbis_stream_process(m_stream, &consumed, &produced);

        std::memmove(m_window, m_window + consumed, m_filled - consumed);
        m_filled -= consumed;
        m_fileOffset += consumed;
        done += produced;

        if (status == RVORBIS_STREAM_ERROR || status == RVORBIS_STREAM_EOS)
            break;

        if (status == RVORBIS_STREAM_NEED_IN)
        {
            // The window is spent: only now is the file touched.
            if (!refill() && !m_filled)
                break;
        }
    }

    return done * BYTES_PER_FRAME;
}

bool OggFile::seekToFrame(uint64_t frame)
{
    size_t fileSize = m_file->size();
    size_t low = 0;
    size_t high = fileSize;

    if (!frame)
    {
        rvorbis_stream_rewind(m_stream);
        m_filled = 0;
        m_fileOffset = 0;
        return m_file->seek(0);
    }

    // Bisect on page granule. Each probe resynchronises at the next page
    // and reports where it landed; nothing is decoded to find out.
    for (int guard = 0; (low < high) && (guard < 64); ++guard)
    {
        size_t mid = low + (high - low) / 2;
        bool landed = false;

        rvorbis_stream_reset(m_stream);
        m_filled = 0;
        if (!m_file->seek(mid))
            return false;

        size_t at = mid;
        int16_t scratch[256 * CD_AUDIO_CHANNELS];

        for (;;)
        {
            size_t consumed = 0;
            size_t produced = 0;

            if (!refill() && !m_filled)
                break;

            rvorbis_stream_set_out_s16(m_stream, scratch, 256);
            rvorbis_stream_set_in(m_stream, m_window, m_filled);

            int status = rvorbis_stream_process(m_stream, &consumed, &produced);

            std::memmove(m_window, m_window + consumed, m_filled - consumed);
            m_filled -= consumed;
            at += consumed;

            if (rvorbis_stream_pos_known(m_stream))
            {
                landed = true;
                break;
            }

            if (status == RVORBIS_STREAM_ERROR || status == RVORBIS_STREAM_EOS)
                break;

            if (!consumed && !produced && !m_filled)
                break;
        }

        if (landed && (rvorbis_stream_tell(m_stream) <= frame))
            low = mid + 1;
        else
            high = mid;
    }

    // One page back, so the target is reached by decoding forward rather
    // than jumped over.
    size_t start = (low > 65307) ? (low - 65307) : 0;

    for (int attempt = 0; attempt < 2; ++attempt)
    {
        if (start)
            rvorbis_stream_reset(m_stream);
        else
            rvorbis_stream_rewind(m_stream);

        m_filled = 0;
        if (!m_file->seek(start))
            return false;

        size_t at = start;
        int16_t scratch[256 * CD_AUDIO_CHANNELS];
        bool overshot = false;

        // Not "while there are bytes left to read": the decoder holds
        // frames after the last byte is consumed, and a target inside
        // them is reached only by continuing to drain.
        for (;;)
        {
            size_t consumed = 0;
            size_t produced = 0;
            size_t want = 256;

            if (rvorbis_stream_pos_known(m_stream))
            {
                uint64_t position = rvorbis_stream_tell(m_stream);

                if (position == frame)
                {
                    m_fileOffset = at;
                    return true;
                }
                if (position > frame)
                {
                    overshot = true;
                    break;
                }
                // Ask for exactly the distance left, or the decode steps
                // past the target and lands wherever the packet ended.
                if ((frame - position) < want)
                    want = static_cast<size_t>(frame - position);
            }

            if (!refill() && !m_filled)
                break;

            rvorbis_stream_set_out_s16(m_stream, scratch, want);
            rvorbis_stream_set_in(m_stream, m_window, m_filled);

            int status = rvorbis_stream_process(m_stream, &consumed, &produced);

            std::memmove(m_window, m_window + consumed, m_filled - consumed);
            m_filled -= consumed;
            at += consumed;

            if (status == RVORBIS_STREAM_ERROR || status == RVORBIS_STREAM_EOS)
                break;

            if (!consumed && !produced && !m_filled)
                break;
        }

        if (!overshot || !start)
            break;

        start = 0;
    }

    return false;
}

bool OggFile::seek(size_t position)
{
    if (!m_isOpen)
        return false;

    return seekToFrame(position / BYTES_PER_FRAME);
}

size_t OggFile::length()
{
    if (!m_isOpen)
        return 0;

    return static_cast<size_t>(m_totalFrames) * BYTES_PER_FRAME;
}

void OggFile::cleanup()
{
    if (m_stream)
    {
        rvorbis_stream_free(m_stream);
        m_stream = nullptr;
    }

    m_file = nullptr;
    m_filled = 0;
    m_fileOffset = 0;
    m_totalFrames = 0;
    m_isOpen = false;
}

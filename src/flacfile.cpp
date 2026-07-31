#include <cstring>

#include "flacfile.h"
#include "libretro_log.h"

constexpr unsigned CD_AUDIO_RATE = 44100;
constexpr unsigned CD_AUDIO_CHANNELS = 2;
constexpr size_t BYTES_PER_FRAME = 4;

FlacFile::FlacFile() :
    m_file(nullptr),
    m_flac(nullptr),
    m_window(),
    m_filled(0),
    m_fileOffset(0),
    m_totalFrames(0),
    m_isOpen(false)
{
}

FlacFile::~FlacFile()
{
    cleanup();
}

bool FlacFile::refill()
{
    if (m_filled >= m_window.size())
        return true;

    size_t got = m_file->readData(m_window.data() + m_filled, m_window.size() - m_filled);
    m_filled += got;

    // Nothing more to come: the decoder may finish the frame it holds
    // rather than wait for a rest that does not exist. A stream's last
    // frame is shorter than the longest one it could be, so without
    // this the tail of every track is held back.
    if (!got)
        rflac_set_eof(m_flac);

    return (got != 0);
}

bool FlacFile::initialize(AbstractFile *file)
{
    cleanup();

    m_file = file;

    if ((!m_file) || (!m_file->isOpen()))
        return false;

    if (!m_file->seek(0))
        return false;

    m_flac = rflac_new();
    if (!m_flac)
        return false;

    m_window.assign(WINDOW_INITIAL, 0);
    m_filled = 0;
    m_fileOffset = 0;

    // Feed until the header is parsed. No output is set, so nothing is
    // decoded before the format is known.
    for (;;)
    {
        size_t consumed = 0;

        if (!refill() && !m_filled)
        {
            cleanup();
            return false;
        }

        // No output is set at all: with none, rflac still parses the
        // header and metadata, which is how the geometry is learned
        // before any buffer is sized.
        rflac_set_in(m_flac, m_window.data(), m_filled);

        int status = rflac_process(m_flac, &consumed, nullptr);

        // Advance by what the decoder took out of the window, which is
        // not what it reports as consumed: bytes pulled into its
        // bitreader cache are gone from the window whether their bits
        // have been used or not, and re-presenting them has them read
        // twice.
        size_t taken = rflac_span_taken(m_flac);

        std::memmove(m_window.data(), m_window.data() + taken, m_filled - taken);
        m_filled -= taken;
        m_fileOffset += taken;

        const rflac_format_t* format = rflac_format(m_flac);
        if (format && format->sample_rate)
        {
            if ((format->channels != CD_AUDIO_CHANNELS)
                || (format->sample_rate != CD_AUDIO_RATE)
                || (format->bits_per_sample != 16))
            {
                Libretro::Log::message(RETRO_LOG_ERROR,
                    "FLAC: %u channels at %uHz / %u bits is not CD audio.\n",
                    format->channels, format->sample_rate, format->bits_per_sample);
                cleanup();
                return false;
            }
            break;
        }

        if (status == RFLAC_PROCESS_ERROR || status == RFLAC_PROCESS_END)
        {
            cleanup();
            return false;
        }
    }

    m_totalFrames = rflac_total_frames(m_flac);

    // Now the geometry is known, grow the window to something a frame
    // always fits in. Below this a rollback can never complete: the
    // decoder puts the part-read frame back and the caller has no room
    // to add the rest.
    size_t minimum = rflac_min_input(m_flac);
    if (m_window.size() < minimum)
        m_window.resize(minimum);

    m_isOpen = true;
    return true;
}

size_t FlacFile::read(char *data, size_t size)
{
    if (!m_isOpen)
        return 0;

    size_t frames = size / BYTES_PER_FRAME;
    size_t done = 0;

    while (done < frames)
    {
        size_t consumed = 0;
        size_t produced = 0;

        // A frame is decoded only once it is present whole. An
        // incomplete one is rolled back into the decoder's carry and
        // the span reported as taken, so the window must be topped up
        // every pass rather than only when nothing came out.
        bool more = refill();

        rflac_set_in(m_flac, m_window.data(), m_filled);
        rflac_set_out_s16(m_flac,
            reinterpret_cast<int16_t*>(data) + done * CD_AUDIO_CHANNELS,
            frames - done);

        int status = rflac_process(m_flac, &consumed, &produced);

        size_t taken = rflac_span_taken(m_flac);

        std::memmove(m_window.data(), m_window.data() + taken, m_filled - taken);
        m_filled -= taken;
        m_fileOffset += taken;
        done += produced;

        if (status == RFLAC_PROCESS_ERROR || status == RFLAC_PROCESS_END)
            break;

        if (!produced && !taken && !more && !m_filled)
            break;
    }

    return done * BYTES_PER_FRAME;
}

bool FlacFile::resumeAt(uint64_t byteOffset, uint64_t frame)
{
    if (!m_file->seek(static_cast<size_t>(byteOffset)))
        return false;

    m_filled = 0;
    m_fileOffset = byteOffset;
    rflac_seek_resumed(m_flac, frame);

    return true;
}

bool FlacFile::decodeForwardTo(uint64_t frame)
{
    int16_t scratch[256 * CD_AUDIO_CHANNELS];

    for (;;)
    {
        uint64_t at = rflac_tell(m_flac);

        if (at >= frame)
            return (at == frame);

        size_t want = 256;
        if ((frame - at) < want)
            want = static_cast<size_t>(frame - at);

        size_t consumed = 0;
        size_t produced = 0;
        bool more = refill();

        rflac_set_in(m_flac, m_window.data(), m_filled);
        rflac_set_out_s16(m_flac, scratch, want);

        int status = rflac_process(m_flac, &consumed, &produced);

        size_t taken = rflac_span_taken(m_flac);

        std::memmove(m_window.data(), m_window.data() + taken, m_filled - taken);
        m_filled -= taken;
        m_fileOffset += taken;

        if (status == RFLAC_PROCESS_ERROR || status == RFLAC_PROCESS_END)
            return false;

        if (!produced && !taken && !more && !m_filled)
            return false;
    }
}

bool FlacFile::seek(size_t position)
{
    if (!m_isOpen)
        return false;

    uint64_t frame = position / BYTES_PER_FRAME;
    uint64_t byteOffset = 0;

    // The seek table is the shortcut; a ripped track usually has none,
    // and then the fallback is to rewind and decode forward.
    if (rflac_seek(m_flac, frame, &byteOffset) == RFLAC_PROCESS_NEXT)
    {
        if (!resumeAt(byteOffset, frame))
            return false;
        if (rflac_tell(m_flac) == frame)
            return true;
        return decodeForwardTo(frame);
    }

    rflac_reset(m_flac);
    m_filled = 0;
    m_fileOffset = 0;
    if (!m_file->seek(0))
        return false;

    return decodeForwardTo(frame);
}

size_t FlacFile::length()
{
    if (!m_isOpen)
        return 0;

    return static_cast<size_t>(m_totalFrames) * BYTES_PER_FRAME;
}

void FlacFile::cleanup()
{
    if (m_flac)
    {
        rflac_free(m_flac);
        m_flac = nullptr;
    }

    m_file = nullptr;
    m_filled = 0;
    m_fileOffset = 0;
    m_totalFrames = 0;
    m_isOpen = false;
}

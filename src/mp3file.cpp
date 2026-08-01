#include <cstring>

#include "libretro_log.h"
#include "mp3file.h"

constexpr unsigned CD_AUDIO_RATE = 44100;
constexpr unsigned CD_AUDIO_CHANNELS = 2;
constexpr size_t BYTES_PER_FRAME = 4;

// An MPEG-1 Layer III frame carries this many PCM frames.
constexpr uint32_t MPEG_FRAME_SAMPLES = 1152;

Mp3File::Mp3File() :
    m_file(nullptr),
    m_stream(nullptr),
    m_window(),
    m_filled(0),
    m_fileOffset(0),
    m_totalFrames(0),
    m_skipFrames(0),
    m_playFrames(0),
    m_position(0),
    m_index(),
    m_eofSent(false),
    m_isOpen(false)
{
}

Mp3File::~Mp3File()
{
    cleanup();
}

// Returns whether anything changed. Signalling end-of-input counts:
// until the decoder has been told, it holds back a short tail rather
// than decode a frame it cannot know is complete, so that signal is
// what lets the last frames out and the caller must go round again.
bool Mp3File::refill()
{
    if (m_filled >= WINDOW_SIZE)
        return true;

    size_t got = m_file->readData(m_window + m_filled, WINDOW_SIZE - m_filled);
    m_filled += got;

    if (got)
        return true;

    if (!m_eofSent)
    {
        rmp3_stream_set_eof(m_stream);
        m_eofSent = true;
        return true;
    }

    return false;
}

bool Mp3File::scan()
{
    if (!m_file->seek(0))
        return false;

    rmp3_stream_reset(m_stream);
    m_filled = 0;
    m_eofSent = false;

    uint64_t frames = 0;
    uint32_t sinceIndex = 0;
    bool first = true;

    for (;;)
    {
        size_t consumed = 0;
        size_t produced = 0;

        // No output: frames are located and counted, none decoded.
        rmp3_stream_set_out_s16(m_stream, nullptr, 0);
        rmp3_stream_set_in(m_stream, m_window, m_filled);

        int status = rmp3_stream_process(m_stream, &consumed, &produced);

        // Read after, not before: the offset is stamped as the frame is
        // located, and in this mode one call is one frame.
        uint64_t offset = rmp3_stream_frame_offset(m_stream);

        std::memmove(m_window, m_window + consumed, m_filled - consumed);
        m_filled -= consumed;

        if (produced)
        {
            uint64_t counted = produced / MPEG_FRAME_SAMPLES;
            if (!counted)
                counted = 1;

            for (uint64_t i = 0; i < counted; ++i)
            {
                if (first || sinceIndex == 0)
                {
                    m_index.push_back(offset);
                    first = false;
                }
                if (++sinceIndex >= INDEX_INTERVAL)
                    sinceIndex = 0;
            }

            frames += produced;
        }

        if (status == RMP3_STREAM_ERROR)
            return false;
        if (status == RMP3_STREAM_END)
            break;

        // Keep going while anything is moving. Once end-of-input has
        // been signalled the decoder still has a hold to drain, and it
        // reports that by producing with nothing consumed - stopping
        // when the window empties would leave that tail unwalked.
        if (!consumed && !produced)
        {
            if (!refill() && !m_filled)
                break;
        }
    }

    m_totalFrames = frames;

    return (m_totalFrames != 0);
}

void Mp3File::parseGapless()
{
    // Enough for an ID3v2 tag of the size a ripper writes plus the
    // frame behind it. A larger one puts the header out of reach and
    // the stream is played as though it had none, which is what a
    // stream without one gets anyway.
    constexpr size_t HEAD = 8192;

    uint8_t head[HEAD];

    m_skipFrames = 0;
    m_playFrames = m_totalFrames;

    if (!m_file->seek(0))
        return;

    size_t got = m_file->readData(head, HEAD);
    size_t at = 0;

    if ((got > 10) && (std::memcmp(head, "ID3", 3) == 0))
    {
        // Syncsafe: seven bits to the byte.
        size_t size = (static_cast<size_t>(head[6] & 0x7F) << 21)
                    | (static_cast<size_t>(head[7] & 0x7F) << 14)
                    | (static_cast<size_t>(head[8] & 0x7F) << 7)
                    | (static_cast<size_t>(head[9] & 0x7F));

        at = 10 + size;

        if (head[5] & 0x10)
            at += 10;
    }

    if ((at + 4) > got)
        return;
    if ((head[at] != 0xFF) || ((head[at + 1] & 0xE0) != 0xE0))
        return;

    unsigned version = (head[at + 1] >> 3) & 0x03;
    unsigned mode = (head[at + 3] >> 6) & 0x03;
    bool mono = (mode == 3);

    // Side information sits between the header and the tag, and how
    // much of it there is depends on both.
    size_t sideInfo;
    if (version == 3)             // MPEG 1
        sideInfo = mono ? 17 : 32;
    else                          // MPEG 2 / 2.5
        sideInfo = mono ? 9 : 17;

    size_t tag = at + 4 + sideInfo;

    if ((tag + 8) > got)
        return;
    if ((std::memcmp(head + tag, "Xing", 4) != 0)
        && (std::memcmp(head + tag, "Info", 4) != 0))
        return;

    uint32_t flags = (static_cast<uint32_t>(head[tag + 4]) << 24)
                   | (static_cast<uint32_t>(head[tag + 5]) << 16)
                   | (static_cast<uint32_t>(head[tag + 6]) << 8)
                   | (static_cast<uint32_t>(head[tag + 7]));

    size_t field = tag + 8;
    uint64_t statedFrames = 0;

    if (flags & 0x01)
    {
        if ((field + 4) > got)
            return;
        statedFrames = (static_cast<uint64_t>(head[field]) << 24)
                     | (static_cast<uint64_t>(head[field + 1]) << 16)
                     | (static_cast<uint64_t>(head[field + 2]) << 8)
                     | (static_cast<uint64_t>(head[field + 3]));
        field += 4;
    }
    if (flags & 0x02)
        field += 4;
    if (flags & 0x04)
        field += 100;
    if (flags & 0x08)
        field += 4;

    // The frame carrying the tag is not one of the stated frames, and
    // it decodes to silence like any other. The walk counted it.
    uint64_t header = MPEG_FRAME_SAMPLES;

    if ((field + 24) > got)
    {
        // A bare Xing header with no encoder extension states the frame
        // count but no delay. Dropping the tag frame is still right.
        if (statedFrames)
        {
            m_skipFrames = header;
            m_playFrames = statedFrames * MPEG_FRAME_SAMPLES;
        }
        return;
    }

    // The extension is only meaningful if an encoder wrote its name in
    // front of it; some muxers leave the space zeroed.
    for (unsigned i = 0; i < 4; ++i)
    {
        uint8_t c = head[field + i];
        if ((c < 0x20) || (c > 0x7E))
        {
            if (statedFrames)
            {
                m_skipFrames = header;
                m_playFrames = statedFrames * MPEG_FRAME_SAMPLES;
            }
            return;
        }
    }

    uint64_t delay = (static_cast<uint64_t>(head[field + 21]) << 4)
                   | (static_cast<uint64_t>(head[field + 22]) >> 4);
    uint64_t padding = (static_cast<uint64_t>(head[field + 22] & 0x0F) << 8)
                     | (static_cast<uint64_t>(head[field + 23]));

    uint64_t decoded = statedFrames
        ? (statedFrames * MPEG_FRAME_SAMPLES)
        : ((m_totalFrames > header) ? (m_totalFrames - header) : 0);

    if (decoded <= (delay + padding))
        return;

    m_skipFrames = header + delay + DECODER_DELAY;
    m_playFrames = decoded - delay - padding;

    // The tail the shift needs comes out of the padding. Where the
    // encoder left less than the filterbank costs, the track runs to
    // the end of what there is instead of past it.
    if ((m_skipFrames + m_playFrames) > m_totalFrames)
        m_playFrames = (m_totalFrames > m_skipFrames)
            ? (m_totalFrames - m_skipFrames) : 0;
}

bool Mp3File::initialize(AbstractFile *file)
{
    cleanup();

    m_file = file;

    if ((!m_file) || (!m_file->isOpen()))
        return false;

    m_stream = rmp3_stream_new();
    if (!m_stream)
        return false;

    if (!scan())
    {
        Libretro::Log::message(RETRO_LOG_ERROR, "MP3: could not walk the stream.\n");
        cleanup();
        return false;
    }

    unsigned channels = 0;
    unsigned rate = 0;

    if (!rmp3_stream_info(m_stream, &channels, &rate))
    {
        cleanup();
        return false;
    }

    // CD audio is 44.1kHz stereo. There is no resampler here, so a
    // stream that is not gets refused rather than played at the wrong
    // speed.
    if ((channels != CD_AUDIO_CHANNELS) || (rate != CD_AUDIO_RATE))
    {
        Libretro::Log::message(RETRO_LOG_ERROR,
            "MP3: %u channels at %uHz is not CD audio (expected %u at %uHz).\n",
            channels, rate, CD_AUDIO_CHANNELS, CD_AUDIO_RATE);
        cleanup();
        return false;
    }

    parseGapless();

    m_isOpen = true;

    return seekToFrame(m_skipFrames);
}

bool Mp3File::seekToFrame(uint64_t frame)
{
    if (m_index.empty())
        return false;

    if (frame > m_totalFrames)
        frame = m_totalFrames;

    // The index is one entry per INDEX_INTERVAL MPEG frames, so this
    // names a frame boundary at or before the target.
    uint64_t mpegFrame = frame / MPEG_FRAME_SAMPLES;
    size_t entry = static_cast<size_t>(mpegFrame / INDEX_INTERVAL);

    if (entry >= m_index.size())
        entry = m_index.size() - 1;

    // Back off so the bit reservoir is warm by the time the target
    // arrives; a frame resumed cold is missing what it carried in.
    size_t warm = (entry > RESERVOIR_WARMUP) ? (entry - RESERVOIR_WARMUP) : 0;

    // An entry at or past the target leaves the decode-forward loop,
    // which only moves forward, no way to reach it. Step back until
    // there is room.
    while (warm > 0
        && static_cast<uint64_t>(warm) * INDEX_INTERVAL * MPEG_FRAME_SAMPLES >= frame)
        --warm;

    uint64_t startMpeg = static_cast<uint64_t>(warm) * INDEX_INTERVAL;

    rmp3_stream_reset(m_stream);
    m_filled = 0;
    m_eofSent = false;

    if (!m_file->seek(static_cast<size_t>(m_index[warm])))
        return false;

    m_fileOffset = m_index[warm];

    // Decode forward to the target, discarding.
    //
    // Position is taken from the decoder rather than counted from what
    // it emits. Resuming partway, some frames decode to nothing - their
    // data began in frames before the resume point, in the bit
    // reservoir - and how many is a property of how the encoder spent
    // its bits, not something derivable here: one frame in some places,
    // three in others. Counting emissions would slide the position by
    // however many those were, which reads as a seek landing late.
    //
    // rmp3_stream_frames_in counts frames consumed whether they emitted
    // or not, so it states where the stream stands regardless.
    int16_t scratch[MPEG_FRAME_SAMPLES * CD_AUDIO_CHANNELS];

    m_position = startMpeg * MPEG_FRAME_SAMPLES;

    // Decode whole frames up to the one holding the target, then take
    // the remainder from inside it. A frame is decoded or it is not;
    // the target rarely sits on a boundary.
    uint64_t targetMpeg = frame / MPEG_FRAME_SAMPLES;

    for (;;)
    {
        uint64_t consumedFrames = rmp3_stream_frames_in(m_stream);
        uint64_t at = startMpeg + consumedFrames;

        if (at >= targetMpeg)
        {
            m_position = at * MPEG_FRAME_SAMPLES;
            break;
        }

        size_t want = MPEG_FRAME_SAMPLES;
        size_t consumed = 0;
        size_t produced = 0;

        rmp3_stream_set_out_s16(m_stream, scratch, want);
        rmp3_stream_set_in(m_stream, m_window, m_filled);

        int status = rmp3_stream_process(m_stream, &consumed, &produced);

        std::memmove(m_window, m_window + consumed, m_filled - consumed);
        m_filled -= consumed;
        m_fileOffset += consumed;

        if (status == RMP3_STREAM_ERROR || status == RMP3_STREAM_END)
            break;

        if (status == RMP3_STREAM_NEED_IN || (!consumed && !produced))
        {
            if (!refill() && !m_filled)
                break;
        }
    }

    // Discard the part of the landing frame that sits before the target.
    while (m_position < frame)
    {
        size_t want = static_cast<size_t>(frame - m_position);
        if (want > MPEG_FRAME_SAMPLES)
            want = MPEG_FRAME_SAMPLES;

        size_t consumed = 0;
        size_t produced = 0;

        rmp3_stream_set_out_s16(m_stream, scratch, want);
        rmp3_stream_set_in(m_stream, m_window, m_filled);

        int status = rmp3_stream_process(m_stream, &consumed, &produced);

        std::memmove(m_window, m_window + consumed, m_filled - consumed);
        m_filled -= consumed;
        m_fileOffset += consumed;
        m_position += produced;

        if (status == RMP3_STREAM_ERROR || status == RMP3_STREAM_END)
            break;

        if (status == RMP3_STREAM_NEED_IN || (!consumed && !produced))
        {
            if (!refill() && !m_filled)
                break;
        }
    }

    return (m_position == frame);
}

size_t Mp3File::read(char *data, size_t size)
{
    if (!m_isOpen)
        return 0;

    size_t frames = size / BYTES_PER_FRAME;
    size_t done = 0;

    // The end of the track, not the end of the stream: what follows is
    // the encoder's padding, which is not part of the recording.
    uint64_t last = m_skipFrames + m_playFrames;

    if (m_position >= last)
        return 0;
    if ((m_position + frames) > last)
        frames = static_cast<size_t>(last - m_position);

    while (done < frames)
    {
        size_t consumed = 0;
        size_t produced = 0;

        rmp3_stream_set_out_s16(m_stream,
            reinterpret_cast<int16_t*>(data) + done * CD_AUDIO_CHANNELS,
            frames - done);
        rmp3_stream_set_in(m_stream, m_window, m_filled);

        int status = rmp3_stream_process(m_stream, &consumed, &produced);

        std::memmove(m_window, m_window + consumed, m_filled - consumed);
        m_filled -= consumed;
        m_fileOffset += consumed;
        done += produced;
        m_position += produced;

        if (status == RMP3_STREAM_ERROR || status == RMP3_STREAM_END)
            break;

        // Top the window up whenever the decoder wants input, not only
        // when nothing at all happened. It holds back a short tail until
        // told no more is coming, and refill() is what tells it - a loop
        // that stops as soon as a call comes up empty never gets there,
        // and the last frames of the stream stay inside the decoder.
        if (status == RMP3_STREAM_NEED_IN || (!consumed && !produced))
        {
            if (!refill() && !m_filled)
                break;
        }
    }

    return done * BYTES_PER_FRAME;
}

bool Mp3File::seek(size_t position)
{
    if (!m_isOpen)
        return false;

    // Positions are in the track; the stream begins before it.
    return seekToFrame((position / BYTES_PER_FRAME) + m_skipFrames);
}

size_t Mp3File::length()
{
    if (!m_isOpen)
        return 0;

    return static_cast<size_t>(m_playFrames) * BYTES_PER_FRAME;
}

void Mp3File::cleanup()
{
    if (m_stream)
    {
        rmp3_stream_free(m_stream);
        m_stream = nullptr;
    }

    m_file = nullptr;
    m_filled = 0;
    m_fileOffset = 0;
    m_totalFrames = 0;
    m_skipFrames = 0;
    m_playFrames = 0;
    m_position = 0;
    m_index.clear();
    m_index.shrink_to_fit();
    m_eofSent = false;
    m_isOpen = false;
}

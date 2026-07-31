#include <algorithm>
#include <cstring>

#include "libretro_log.h"
#include "wavfile.h"

constexpr unsigned CD_AUDIO_RATE = 44100;
constexpr unsigned CD_AUDIO_CHANNELS = 2;
constexpr int64_t BYTES_PER_FRAME = 4;

// A RIFF chunk list is a few dozen bytes in almost every file; the cap
// is for the pathological ones carrying large LIST or id3 chunks ahead
// of the audio.
constexpr size_t HEAD_INITIAL = 8192;
constexpr size_t HEAD_MAX = 1u << 20;

WavFile::WavFile() :
    m_file(nullptr),
    m_wav(),
    m_chunk(),
    m_currentFrame(0),
    m_totalFrames(0),
    m_isOpen(false)
{
}

WavFile::~WavFile()
{
    cleanup();
}

bool WavFile::parseHeader()
{
    size_t fileSize = m_file->size();
    std::vector<uint8_t> head;

    for (size_t want = HEAD_INITIAL; ; want *= 2)
    {
        if (want > fileSize)
            want = fileSize;

        head.resize(want);

        if (!m_file->seek(0))
            return false;
        if (m_file->readData(head.data(), want) != want)
            return false;

        // Bounded by what is actually resident: rwav stops at the end of
        // the 'data' chunk header, so a successful parse means the whole
        // header was inside this head and nothing was read past it.
        if (rwav_parse(&m_wav, head.data(), want) == RWAV_ITERATE_DONE)
            break;

        if ((want >= fileSize) || (want >= HEAD_MAX))
            return false;
    }

    // subchunk2size came back clamped to the head, since that is all
    // that was addressable. The payload runs to the end of the file, so
    // restate it against the real size and rederive the frame count.
    if (m_wav.dataoffset >= fileSize)
        return false;

    size_t payload = fileSize - m_wav.dataoffset;
    if (!m_wav.blockalign)
        return false;

    size_t units = payload / m_wav.blockalign;
    m_wav.subchunk2size = units * m_wav.blockalign;

    if (m_wav.samplesperblock > 1)
        m_wav.numsamples = units * m_wav.samplesperblock;
    else
        m_wav.numsamples = units;

    m_wav.samples = nullptr;

    return (m_wav.numsamples != 0);
}

bool WavFile::initialize(AbstractFile *file)
{
    cleanup();

    m_file = file;

    if ((!m_file) || (!m_file->isOpen()))
        return false;

    if (!parseHeader())
        return false;

    // CD audio is 44.1kHz stereo. The encoding no longer has to be raw
    // 16-bit PCM - rwav normalises 24-bit, float, A-law, mu-law and the
    // ADPCM forms to s16 - but the rate and channel count are what the
    // playback pipeline is built around, and there is no resampler.
    if ((m_wav.numchannels != CD_AUDIO_CHANNELS) || (m_wav.samplerate != CD_AUDIO_RATE))
    {
        Libretro::Log::message(RETRO_LOG_ERROR,
            "WAV: %u channels at %uHz is not CD audio (expected %u at %uHz).\n",
            m_wav.numchannels, m_wav.samplerate, CD_AUDIO_CHANNELS, CD_AUDIO_RATE);
        return false;
    }

    m_currentFrame = 0;
    m_totalFrames = static_cast<int64_t>(m_wav.numsamples);
    m_isOpen = true;

    return true;
}

int64_t WavFile::read(void *data, int64_t size)
{
    if (!m_isOpen)
        return 0;

    int64_t frames = size / BYTES_PER_FRAME;
    int64_t available = m_totalFrames - m_currentFrame;

    if (frames > available)
        frames = available;
    if (frames <= 0)
        return 0;

    size_t offset = 0;
    size_t length = 0;

    // Which bytes this frame range needs. For the block-coded formats
    // that is whole coded blocks, which only rwav can locate.
    if (!rwav_frame_extent(&m_wav, static_cast<size_t>(m_currentFrame),
            static_cast<size_t>(frames), &offset, &length))
        return 0;

    if (m_chunk.size() < length)
        m_chunk.resize(length);

    if (!m_file->seek(offset))
        return 0;
    if (m_file->readData(m_chunk.data(), length) != length)
        return 0;

    size_t got = rwav_decode_s16_at(&m_wav, m_chunk.data(), offset,
        static_cast<size_t>(m_currentFrame), static_cast<size_t>(frames),
        reinterpret_cast<short*>(data));

    m_currentFrame += static_cast<int64_t>(got);

    return static_cast<int64_t>(got) * BYTES_PER_FRAME;
}

bool WavFile::seek(int64_t position)
{
    if (!m_isOpen)
        return false;

    int64_t frame = position / BYTES_PER_FRAME;

    // Frame-addressed rather than byte-addressed: for a block-coded
    // payload a byte offset does not name a frame at all.
    m_currentFrame = std::min(frame, m_totalFrames);

    return true;
}

int64_t WavFile::length()
{
    if (!m_isOpen)
        return 0;

    return m_totalFrames * BYTES_PER_FRAME;
}

void WavFile::cleanup()
{
    m_file = nullptr;
    m_chunk.clear();
    m_chunk.shrink_to_fit();
    std::memset(&m_wav, 0, sizeof(m_wav));
    m_currentFrame = 0;
    m_totalFrames = 0;
    m_isOpen = false;
}

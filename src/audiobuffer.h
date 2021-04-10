#ifndef AUDIOBUFFER_H
#define AUDIOBUFFER_H

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>

#include "clamp.h"
#include "round.h"
#include "timer.h"

class AudioBuffer
{
public:
    /// The sample rate of generated audio
    static constexpr uint32_t SAMPLE_RATE = 44100;
    
    /// How many samples to generate for each frame
    static constexpr double SAMPLES_PER_FRAME = static_cast<double>(SAMPLE_RATE) / Timer::FRAME_RATE;

    /// The audio buffer size, enough for one frame of audio.
    static constexpr uint32_t CD_BUFFER_SIZE = round<int32_t>(SAMPLES_PER_FRAME + 1.0);

    /// We may occasionally generate one or two extra samples because of a long instruction (like DIVU)
    static constexpr uint32_t YM_BUFFER_SIZE = CD_BUFFER_SIZE + 2;

    struct Sample
    {
        int16_t left;
        int16_t right;
    };

    static inline int16_t saturatedAdd(int16_t a, int16_t b)
    {
        return std::clamp(
            static_cast<int32_t>(a) + static_cast<int32_t>(b),
            static_cast<int32_t>(std::numeric_limits<int16_t>::min()),
            static_cast<int32_t>(std::numeric_limits<int16_t>::max()));
    }

    AudioBuffer()
    {
        reset();
    }
    
    void reset()
    {
        sampleCount = 0;
        hasCdAudio = false;
        cdSamples.fill({ 0, 0 });
        ymSamples.fill({ 0, 0 });
        writePointer = 0;
    }

    void initialize(uint32_t samples, bool hasCd)
    {
        // TODO: Remove this
        if (samples > CD_BUFFER_SIZE)
            std::abort();

        // We generated too many samples on the last frame
        if (writePointer > sampleCount)
        {
            // Move the extra samples to the beginning and adjust the write pointer
            std::copy(ymSamples.begin() + sampleCount, ymSamples.begin() + writePointer, ymSamples.begin());
            writePointer -= sampleCount;
        }
        else
            writePointer = 0;

        sampleCount = samples;
        hasCdAudio = hasCd;
    }

    inline void appendSample(const Sample& sample)
    {
        // TODO: Remove this
        if (writePointer > YM_BUFFER_SIZE)
            std::abort();

        ymSamples[writePointer] = sample;
        ++writePointer;
    }

    void mix()
    {
        // No audio CD samples = nothing to do
        if (!hasCdAudio)
            return;

        // Mix the two audio buffers
        std::transform(ymSamples.cbegin(), ymSamples.cbegin() + sampleCount, cdSamples.cbegin(), ymSamples.begin(), [](const Sample& a, const Sample& b) -> Sample
        {
            return {
                saturatedAdd(a.left, b.left),
                saturatedAdd(a.right, b.right)
            };
        });
    }

    int32_t masterCyclesThisFrameToSample(int32_t cycles) const
    {
        return static_cast<double>(sampleCount) * static_cast<double>(cycles) / static_cast<double>(Timer::CYCLES_PER_FRAME);
    }

    int32_t masterCyclesThisFrameToSampleClamped(int32_t cycles) const
    {
        return std::clamp(masterCyclesThisFrameToSample(cycles), int32_t(0), static_cast<int32_t>(sampleCount - 1));
    }

    /// How many samples to generate this frame (this can vary because of rounding)
    uint32_t sampleCount;

    /// This bool is true if there is cd audio to mix for this frame
    bool hasCdAudio;
    
    /// Buffer for the generated audio
    std::array<Sample, CD_BUFFER_SIZE> cdSamples;
    
    /// Buffer for the generated audio
    std::array<Sample, YM_BUFFER_SIZE> ymSamples;

    /// Write index for audio data
    uint32_t writePointer;
};

#endif // AUDIOBUFFER_H

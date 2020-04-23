#include "3rdparty/ym/ym2610.h"
#include "3rdparty/z80/z80.h"
#include "audio.h"
#include "neogeocd.h"
#include "timer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

// Replace with the std version once c++17 is finally widely available
template <typename T>
constexpr const T& clamp(const T& val, const T& min, const T& max)
{
    return std::max(min, std::min(max, val));
}

void YM2610UpdateRequest(void)
{
    if (neocd.audio.currentSample > neocd.audio.audioWritePointer + 4)
        YM2610Update(neocd.audio.currentSample - neocd.audio.audioWritePointer);
}

void YM2610TimerHandler(int channel, int count, double steptime)
{
    double      time_seconds;
    uint32_t    time_cycles;

    if (!count)
    {
        if (!channel)
            neocd.timers.ym2610TimerA->setState(Timer::Stopped);
        else
            neocd.timers.ym2610TimerB->setState(Timer::Stopped);
    }
    else
    {
        time_seconds = (double)count * steptime;
        time_cycles = (uint32_t)Timer::secondsToMaster(time_seconds);

        if (!channel)
            neocd.timers.ym2610TimerA->arm(time_cycles);
        else
            neocd.timers.ym2610TimerB->arm(time_cycles);
    }
}

void YM2610IrqHandler(int irq)
{
    if (irq)
        z80_set_irq_line(0, ASSERT_LINE);
    else
        z80_set_irq_line(0, CLEAR_LINE);
}

Audio::Audio() :
    hasCdAudioThisFrame(false),
    samplesThisFrameF(0.0),
    samplesThisFrame(0),
    currentSample(0),
    audioWritePointer(0)
{
    reset();
}

void Audio::reset()
{
    std::memset(cdAudioBuffer, 0, sizeof(cdAudioBuffer));
    std::memset(audioBuffer, 0, sizeof(audioBuffer));
    samplesThisFrameF = 0.0;
    samplesThisFrame = 0;
    currentSample = 0;
    hasCdAudioThisFrame = false;
    audioWritePointer = 0;
}

void Audio::initFrame()
{
    samplesThisFrameF += (static_cast<double>(SAMPLE_RATE) / Timer::FRAME_RATE);
    samplesThisFrame = static_cast<int>(std::ceil(samplesThisFrameF));
    samplesThisFrameF -= samplesThisFrame;
    audioWritePointer = 0;

    if (neocd.cdrom.isPlaying() && neocd.cdrom.isAudio())
    {
        hasCdAudioThisFrame = true;
        neocd.cdrom.readAudio(reinterpret_cast<char*>(cdAudioBuffer), samplesThisFrame * 4);
    }
    else
        hasCdAudioThisFrame = false;
}

void Audio::updateCurrentSample()
{
    currentSample = static_cast<uint32_t>(0.5 + (static_cast<double>(Timer::CYCLES_PER_FRAME - std::max<int32_t>(0, neocd.remainingCyclesThisFrame)) * (samplesThisFrame - 1) / Timer::CYCLES_PER_FRAME));
}

void Audio::finalize()
{
    // Generate YM2610 samples
    if (audioWritePointer < samplesThisFrame)
        YM2610Update(samplesThisFrame - audioWritePointer);

    // If we have audio CD samples...
    if (!hasCdAudioThisFrame)
        return;

    // ...mix the two audio buffers
    std::transform(audioBuffer, audioBuffer + (samplesThisFrame * 2), cdAudioBuffer, audioBuffer, [](int16_t sampleA, int16_t sampleB) {
        return static_cast<int16_t>(clamp(
            static_cast<int32_t>(sampleA) + static_cast<int32_t>(sampleB),
            static_cast<int32_t>(std::numeric_limits<int16_t>::min()),
            static_cast<int32_t>(std::numeric_limits<int16_t>::max())));
    });
}

DataPacker& operator<<(DataPacker& out, const Audio& audio)
{
    out << audio.samplesThisFrameF;
    return out;
}

DataPacker& operator>>(DataPacker& in, Audio& audio)
{
    in >> audio.samplesThisFrameF;
    return in;
}

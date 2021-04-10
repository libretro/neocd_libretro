#include <algorithm>
#include <cmath>
#include <cstring>

#include "3rdparty/ym/ym2610.h"
#include "3rdparty/z80/z80.h"
#include "audio.h"
#include "neogeocd.h"
#include "timer.h"

void YM2610UpdateRequest(void)
{
    const int32_t currentSample = neocd.audio.buffer.masterCyclesThisFrameToSample(neocd.z80CyclesThisFrame());

    if (currentSample > int32_t(neocd.audio.buffer.writePointer))
        YM2610Update(currentSample - neocd.audio.buffer.writePointer);
}

void YM2610TimerHandler(int channel, int count, double steptime)
{
    auto& timer = (channel == 0) ? neocd.timers.timer<TimerGroup::Ym2610A>() : neocd.timers.timer<TimerGroup::Ym2610B>();

    if (!count)
        timer.setState(Timer::Stopped);
    else
    {
        const double time_seconds = (double)count * steptime;
        const uint32_t time_cycles = (uint32_t)Timer::secondsToMaster(time_seconds);

        timer.arm(time_cycles);
    }
}

void YM2610IrqHandler(int irq)
{
    z80_set_irq_line(0, irq ? ASSERT_LINE : CLEAR_LINE);
}

Audio::Audio() :
    samplesThisFrameF(0.0),
    buffer()
{
    reset();
}

void Audio::reset()
{
    samplesThisFrameF = 0.0;
    buffer.reset();
}

void Audio::initFrame()
{
    samplesThisFrameF += AudioBuffer::SAMPLES_PER_FRAME;

    const int samplesThisFrame = static_cast<int>(std::ceil(samplesThisFrameF));
    samplesThisFrameF -= samplesThisFrame;

    const bool hasCdAudio = neocd.cdrom.isPlaying() && neocd.cdrom.isAudio();

    buffer.initialize(samplesThisFrame, hasCdAudio);

    if (hasCdAudio)
        neocd.cdrom.readAudio(reinterpret_cast<char*>(&buffer.cdSamples[0]), samplesThisFrame * sizeof(AudioBuffer::Sample));
}

void Audio::finalize()
{
    // Generate YM2610 samples
    if (buffer.writePointer < buffer.sampleCount)
        YM2610Update(buffer.sampleCount - buffer.writePointer);

    // If we have audio CD samples...
    if (!buffer.hasCdAudio)
        return;

    // ...mix the two audio buffers
    buffer.mix();
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

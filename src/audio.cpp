#include <algorithm>
#include <cmath>
#include <cstring>

#include "3rdparty/ym/ym2610.h"
#include "3rdparty/z80/z80.h"
#include "audio.h"
#include "libretro_common.h"
#include "neogeocd.h"
#include "timer.h"

void YM2610UpdateRequest(void)
{
    const int32_t currentSample = neocd->audio.buffer.masterCyclesThisFrameToSample(neocd->z80CyclesThisFrame());

    if (currentSample > int32_t(neocd->audio.buffer.writePointer))
        YM2610Update(currentSample - neocd->audio.buffer.writePointer);
}

void YM2610TimerHandler(int channel, int count)
{
    auto& timer = (channel == 0) ? neocd->timers.timer<TimerGroup::Ym2610A>() : neocd->timers.timer<TimerGroup::Ym2610B>();

    if (!count)
        timer.setState(Timer::Stopped);
    else
    {
        /* count ticks of the chip's timer clock, 8000000 / 144, in
           master clock cycles: the ratio to the 24168000 master is
           exactly 144 * 3021 / 1000 per tick, and the count never
           makes the product big enough to care about the order. Half
           is added before the divide because the old path rounded to
           nearest, and no count can land exactly on a half.
        */
        const uint32_t time_cycles = (uint32_t)(((uint64_t)count * 144 * 3021 + 500) / 1000);

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

    const bool hasCdAudio = neocd->cdrom.isPlaying() && neocd->cdrom.isAudio();

    buffer.initialize(samplesThisFrame, hasCdAudio);

    if (hasCdAudio)
        neocd->cdrom.readAudio(reinterpret_cast<char*>(&buffer.cdSamples[0]), samplesThisFrame * sizeof(AudioBuffer::Sample));
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

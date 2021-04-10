#ifndef AUDIO_H
#define AUDIO_H

#include <cstdint>

#include "3rdparty/ym/ym2610.h"
#include "audiobuffer.h"
#include "datapacker.h"
#include "timer.h"

extern void YM2610UpdateRequest(void);
extern void YM2610TimerHandler(int channel, int count, double steptime);
extern void YM2610IrqHandler(int irq);

/**
 * @class Audio
 * @brief The audio class generates audio samples that are then passed to RetroArch
 */
class Audio
{
public:
    /// The sample rate of generated audio
    static constexpr uint32_t SAMPLE_RATE = 44100;
    
    /// The audio buffer size, enough for one frame of audio.
    static constexpr uint32_t BUFFER_SIZE = static_cast<uint32_t>(static_cast<double>(SAMPLE_RATE * Timer::SCREEN_WIDTH) / (Timer::PIXEL_CLOCK / static_cast<double>(Timer::SCREEN_HEIGHT)) + 1.0);
    
    Audio();
    
    /// Non copyable
    Audio(const Audio&) = delete;

    /// Non copyable
    void operator=(const Audio&) = delete;
    
    /**
     * @brief Reset all members to a known state
     */
    void reset();

    /**
     * @brief Start generating a frame of audio.
     * This will get CD audio data from the CDROM class.
     * CD audio is loaded and decoded in a separate thread.
     */
    void initFrame();

    /**
     * @brief Generate YM2610 samples for this frame and mix with CD audio if needed
     */
    void finalize();

    /// How many samples to generate this frame (this can vary because of rounding)
    double samplesThisFrameF;

    /// Buffer to store ym audio and cd audio
    AudioBuffer buffer;
};

/**
 * @brief Save Audio state to a DataPacker
 * @param out DataPacker to save to
 * @param audio The audio state
 */
DataPacker& operator<<(DataPacker& out, const Audio& audio);

/**
 * @brief Restore Audio state from a DataPacker
 * @param in DataPacker to read state from
 * @param audio The audio state
 */
DataPacker& operator>>(DataPacker& in, Audio& audio);

#endif // AUDIO_H

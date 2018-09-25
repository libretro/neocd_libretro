#ifndef AUDIO_H
#define AUDIO_H

#include "timer.h"
#include "3rdparty/ym/ym2610.h"
#include "datapacker.h"

#include <cstdint>

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
     * @brief Calculate the sample number corresponding to the current execution time.
     */
    void updateCurrentSample();

    /**
     * @brief Generate YM2610 samples for this frame and mix with CD audio if needed
     */
    void finalize();

    /// This bool is true if there is cd audio to mix for this frame
    bool        hasCdAudioThisFrame;
    
    /// Buffer for the generated audio
    int16_t     cdAudioBuffer[BUFFER_SIZE * 2];
    
    /// Buffer for the generated audio
    int16_t     audioBuffer[BUFFER_SIZE * 2];
    
    /// How many samples to generate this frame (this can vary because of rounding)
    double      samplesThisFrameF;
    
    /// How many samples to generate this frame (this can vary because of rounding)
    uint32_t    samplesThisFrame;
    
    /// The index of the sample corresponding to the current emulated time
    uint32_t    currentSample;
    
    /// Write index for audio data
    uint32_t    audioWritePointer;
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

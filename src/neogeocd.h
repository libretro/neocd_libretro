#ifndef NEOGEOCD_H
#define NEOGEOCD_H

#include <cstddef>

#include "audio.h"
#include "bios.h"
#include "cdrom.h"
#include "datapacker.h"
#include "input.h"
#include "lc8951.h"
#include "libretro.h"
#include "memory.h"
#include "misc.h"
#include "timergroup.h"
#include "video.h"

class NeoGeoCD
{
public:

    enum Nationality
    {
        NationalityJapan = 0,
        NationalityUSA = 1,
        NationalityEurope = 2,
        NationalityPortugal = 3
    };

    enum Interrupt {
        VerticalBlank = 1,
        CdromDecoder = 2,
        CdromCommunication = 4,
        Raster = 8
    };

    NeoGeoCD();
    
    // Non copyable
    NeoGeoCD(const NeoGeoCD&) = delete;
    
    // Non copyable
    NeoGeoCD& operator=(const NeoGeoCD&) = delete;
    
    void initialize();
    void deinitialize();

    void reset();
    void runOneFrame();

    void setInterrupt(NeoGeoCD::Interrupt interrupt);
    void clearInterrupt(NeoGeoCD::Interrupt interrupt);
    int  updateInterrupts();
    
    inline int  getScreenX() const
    {
        return (Timer::masterToPixel(Timer::CYCLES_PER_FRAME - remainingCyclesThisFrame) % Timer::SCREEN_WIDTH);
    }

    inline int  getScreenY() const
    {
        return (Timer::masterToPixel(Timer::CYCLES_PER_FRAME - remainingCyclesThisFrame) / Timer::SCREEN_WIDTH);
    }

    inline bool isCdDecoderIRQEnabled() const
    {
        return ((irqMask1 & 0x500) == 0x500);
    }
    
    inline bool isCdCommunicationIRQEnabled() const
    {
        return ((irqMask1 & 0x50) == 0x50) && cdCommunicationNReset;
    }

    inline bool isVBLEnabled() const
    {
        return (irqMask2 & 0x030) == 0x030;
    }

    inline bool isHBLEnabled() const
    {
        return (irqMask2 & 0x700) == 0x700;
    }

    inline bool isCDZ() const
    {
        return biosType == Bios::CDZ;
    }

    int32_t m68kMasterCyclesThisFrame() const;

    int32_t z80CyclesRun() const;

    double z80CurrentTimeSeconds() const;

    int32_t z80CyclesThisFrame() const;

    bool saveState(DataPacker& out) const;
    bool restoreState(DataPacker& in);

    Memory memory;
    Video video;
    Cdrom cdrom;
    LC8951 lc8951;
    TimerGroup timers;
    Input input;
    Audio audio;

    // Variables to save in savestate
    uint32_t    cdzIrq1Divisor;
    bool        cdCommunicationNReset;
    uint32_t    irqMask1;
    uint32_t    irqMask2;
    bool        irq1EnabledThisFrame;
    bool        fastForward;
    uint32_t    machineNationality;
    uint32_t    cdromVector;
    uint32_t    pendingInterrupts;
    int32_t     remainingCyclesThisFrame;
    int32_t     z80TimeSlice;
    bool        z80Disable;
    bool        z80NMIDisable;
    double      currentTimeSeconds;
    uint32_t    audioCommand;
    uint32_t    audioResult;
    uint32_t    biosType;
    // End variables to save in savestate
};

extern NeoGeoCD neocd;

struct LibretroCallbacks
{
    retro_log_printf_t log;
    retro_video_refresh_t video;
    retro_input_poll_t inputPoll;
    retro_input_state_t inputState;
    retro_environment_t environment;
    retro_audio_sample_batch_t audioBatch;
    retro_perf_callback perf;
};

extern LibretroCallbacks libretro;

#endif // NEOGEOCD_H

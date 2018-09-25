#ifndef NEOGEOCD_H
#define NEOGEOCD_H

#include <cstddef>

#include "memory.h"
#include "video.h"
#include "cdrom.h"
#include "lc8951.h"
#include "timergroup.h"
#include "input.h"
#include "audio.h"
#include "libretro.h"
#include "misc.h"
#include "datapacker.h"

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

    enum BiosType {
        FrontLoader = 0,
        TopLoader = 1,
        CDZ = 2
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
    
    int  getScreenX() const;
    int  getScreenY() const;

    inline bool isIRQ1Enabled() const
    {
        return ((irqMask1 & 0x500) == 0x500) && irqMasterEnable;
    }
    
    inline bool isIRQ2Enabled() const
    {
        return ((irqMask1 & 0x50) == 0x50) && irqMasterEnable;
    }

    inline bool isCDZ() const
    {
        return biosType == NeoGeoCD::CDZ;
    }

    inline bool isVBLEnabled() const
    {
        return (irqMask2 & 0x030) == 0x030;
    }
    
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
    bool        irqMasterEnable;
    uint32_t    irqMask1;
    uint32_t    irqMask2;
    bool        irq1EnabledThisFrame;
    bool        fastForward;
    uint32_t    machineNationality;
    uint32_t    cdromVector;
    uint32_t    pendingInterrupts;
    int32_t     remainingCyclesThisFrame;
    int32_t     m68kCyclesThisFrame;
    int32_t     z80CyclesThisFrame;
    int32_t     z80TimeSlice;
    bool        z80Disable;
    bool        z80NMIDisable;
    double      currentTimeSeconds;
    uint32_t    audioCommand;
    uint32_t    audioResult;
    BiosType    biosType;
    
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

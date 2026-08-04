#ifndef NEOGEOCD_H
#define NEOGEOCD_H

#include <cstddef>

#include "audio.h"
#include "bios.h"
#include "cdrom.h"
#include "datapacker.h"
#include "input.h"
#include "lc8951.h"
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

    ~NeoGeoCD();

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

#define ADJUST_FRAME_BOUNDARY
    inline int  getScreenX() const
    {
#ifndef ADJUST_FRAME_BOUNDARY
        return (Timer::masterToPixel(Timer::CYCLES_PER_FRAME - remainingCyclesThisFrame) % Timer::SCREEN_WIDTH);
#else
        return (Timer::VBL_IRQ_X + Timer::masterToPixel(Timer::CYCLES_PER_FRAME - remainingCyclesThisFrame)) % Timer::SCREEN_WIDTH;
#endif
    }

    /// Master cycles the 68000 has run inside the timeslice currently
    /// being executed, or zero between slices. The frame's remaining
    /// cycle count only moves when a slice ends, so without this the
    /// beam stands still for everyone who looks at it from inside one:
    /// a game that polls the raster counter waiting for a line sees the
    /// same line for the whole slice, misses its moment, and spends the
    /// rest of the frame waiting for a wrap that already happened. Neo
    /// Drift Out times its road effect that way, and stuttered at every
    /// other frame for it.
    int32_t midSliceElapsed() const;

    inline int  getScreenY() const
    {
#ifndef ADJUST_FRAME_BOUNDARY
        return (Timer::masterToPixel(Timer::CYCLES_PER_FRAME - remainingCyclesThisFrame + midSliceElapsed()) / Timer::SCREEN_WIDTH);
#else
        return (Timer::VBL_IRQ_Y + (Timer::VBL_IRQ_X + Timer::masterToPixel(Timer::CYCLES_PER_FRAME - remainingCyclesThisFrame + midSliceElapsed())) / Timer::SCREEN_WIDTH) % Timer::SCREEN_HEIGHT;
#endif
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
        return true;
    }

    inline bool isCDZ() const
    {
        return biosType == Bios::CDZ;
    }

    int32_t m68kMasterCyclesThisFrame() const;

    int32_t z80CyclesRun() const;

    uint64_t z80CurrentTimeCycles() const;

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
    bool        cdSectorDecodedThisFrame;
    bool        fastForward;
    uint32_t    machineNationality;
    uint32_t    cdromVector;
    uint32_t    pendingInterrupts;
    int32_t     remainingCyclesThisFrame;
    int32_t     z80TimeSlice;
    /* Remainder of the overclock division, in hundredths of a master
       cycle, carried from one timeslice into the next so the scaling
       neither creates nor loses time. Deliberately not in the saved
       state - it is a fraction of a cycle - but it is machine state
       all the same, so reset clears it and a restore starts it from
       zero rather than from whatever this session had reached.
    */
    uint32_t    cpuOverclockCarry;
    bool        z80Disable;
    bool        z80NMIDisable;
    /* Master clock cycles since power on, whole ones; the fractions a
       double used to carry served nothing but the sound chip's busy
       flag, which counts in cycles anyway. Same eight bytes in the
       saved state.
    */
    uint64_t    currentTimeCycles;
    uint32_t    audioCommand;
    uint32_t    audioResult;
    uint32_t    biosType;
    // End variables to save in savestate

    /// True when no BIOS file was found and the stand-in is in use.
    /// Not saved: it follows from how the core was started, not from
    /// where the machine has got to.
    bool        usingHleBios;
};

#endif // NEOGEOCD_H

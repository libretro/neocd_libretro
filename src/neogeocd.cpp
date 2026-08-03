#include <algorithm>

#include "3rdparty/ym/ym2610.h"
#include "3rdparty/z80/z80.h"
#include "hlebios.h"
#include "libretro_common.h"
#include "m68kintf.h"
#include "neogeocd.h"
#include "timer.h"
#include "z80intf.h"
#include "libretro_log.h"

extern "C"
{
    #include "3rdparty/musashi/m68kcpu.h"
}

NeoGeoCD::NeoGeoCD() :
    memory(),
    video(),
    cdrom(),
    lc8951(),
    timers(),
    input(),
    audio(),
    cdzIrq1Divisor(0),
    cdCommunicationNReset(false),
    irqMask1(0),
    irqMask2(0),
    cdSectorDecodedThisFrame(false),
    fastForward(false),
    machineNationality(NationalityJapan),
    cdromVector(0),
    pendingInterrupts(0),
    remainingCyclesThisFrame(0),
    z80TimeSlice(0),
    z80Disable(true),
    z80NMIDisable(true),
    currentTimeSeconds(0.0),
    audioCommand(0),
    audioResult(0),
    biosType(Bios::Unknown),
    usingHleBios(false)
{
    // Create the worker thread to buffer & decode audio data
    cdrom.createWorkerThread();
}

NeoGeoCD::~NeoGeoCD()
{
    // End the worker thread
    cdrom.endWorkerThread();
}

void NeoGeoCD::initialize()
{
    // Initialize the 68000 emulation core
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    m68k_init();


    // Inizialize the z80 core
    z80_init(0, Timer::Z80_CLOCK, NULL, z80_irq_callback);

    // Initialize the YM2610
    YM2610Init(8000000, Audio::SAMPLE_RATE, memory.pcmRam, Memory::PCMRAM_SIZE, YM2610TimerHandler, YM2610IrqHandler);

    // Reset everything
    reset();
}

void NeoGeoCD::deinitialize()
{
}

void NeoGeoCD::reset()
{
    memory.reset();
    video.reset();
    cdrom.reset();
    lc8951.reset();
    timers.reset();
    input.reset();
    audio.reset();

    cdromVector = 0;
    cdCommunicationNReset = false;
    cdzIrq1Divisor = 0;
    pendingInterrupts = 0;
    irqMask1 = 0;
    irqMask2 = 0;
    remainingCyclesThisFrame = 0;
    z80TimeSlice = 0;
    z80Disable = true;
    z80NMIDisable = true;
    currentTimeSeconds = 0.0;
    fastForward = false;
    audioCommand = 0;
    audioResult = 0;

    m68k_pulse_reset();
    z80_reset();
    YM2610Reset();
}

void NeoGeoCD::runOneFrame()
{
    remainingCyclesThisFrame += Timer::CYCLES_PER_FRAME;

    audio.initFrame();

    while (remainingCyclesThisFrame > 0)
    {
        uint32_t timeSlice = std::min(timers.timeSlice(), remainingCyclesThisFrame);
        uint32_t elapsed   = Timer::m68kToMaster(m68k_execute(Timer::masterToM68k(timeSlice)));

        z80TimeSlice += elapsed;
        if (z80TimeSlice > 0)
        {
            uint32_t z80Elapsed;

            if (z80Disable)
                z80Elapsed = z80TimeSlice;
            else
                z80Elapsed = Timer::z80ToMaster(z80_execute(Timer::masterToZ80(z80TimeSlice)));

            z80TimeSlice -= z80Elapsed;
        }

        remainingCyclesThisFrame -= elapsed;
        currentTimeSeconds += Timer::masterToSeconds(elapsed);

        timers.advanceTime(elapsed);
    }

    audio.finalize();
}

void NeoGeoCD::setInterrupt(NeoGeoCD::Interrupt interrupt)
{
    pendingInterrupts |= interrupt;
}

void NeoGeoCD::clearInterrupt(NeoGeoCD::Interrupt interrupt)
{
    pendingInterrupts &= ~interrupt;
}

/**
 * Interrupt levels have been determined by hooking each interrupt and writing SR somewhere in memory.
 * VBL              -> SR=2100
 * CD Communication -> SR=2200
 * CD Decoder       -> SR=2200
 * HBL              -> SR=2300
 *
 * The CD communication interrupt frequency changes:
 * It happens at about 64Hz if the CD-ROM is idle, or exactly 75Hz if the CD-ROM is reading.
 *
 * Timings
 * =======
 *
 * Measured by hooking the interrupt and incrementing a memory location.
 * 10 minutes measured with a stopwatch (tried to be as accurate as possible)
 *
 * VBL
 * 10 minutes = 35759 interrupts -> ~59.5983Hz
 *
 * CD Communication (Nothing playing)
 * 10 minutes = 38788 interrupts -> ~64.6466Hz
 *
 */
int NeoGeoCD::updateInterrupts(void)
{
    int level = 0;

    if (pendingInterrupts & NeoGeoCD::VerticalBlank)
        level = 1;

    if (pendingInterrupts & NeoGeoCD::CdromDecoder)
    {
        level = 2;
        cdromVector = 0x54;
    }

    if (pendingInterrupts & NeoGeoCD::CdromCommunication)
    {
        level = 2;
        cdromVector = 0x58;
    }

    if (pendingInterrupts & NeoGeoCD::Raster)
        level = 3;

    m68k_set_irq(level);

    return level;
}

int32_t NeoGeoCD::m68kMasterCyclesThisFrame() const
{
    return Timer::CYCLES_PER_FRAME - remainingCyclesThisFrame + Timer::m68kToMaster(m68k_cycles_run());
}

int32_t NeoGeoCD::z80CyclesRun() const
{
   return z80TimeSlice - Timer::z80ToMaster(z80_ICount);
}

double NeoGeoCD::z80CurrentTimeSeconds() const
{
    return currentTimeSeconds + Timer::masterToSeconds(z80CyclesRun());
}

int32_t NeoGeoCD::z80CyclesThisFrame() const
{
    return Timer::CYCLES_PER_FRAME - remainingCyclesThisFrame + z80CyclesRun();
}

bool NeoGeoCD::saveState(DataPacker& out) const
{
    // General machine state
    out << cdzIrq1Divisor;
    out << cdCommunicationNReset;
    out << irqMask1;
    out << irqMask2;
    out << cdSectorDecodedThisFrame;
    out << fastForward;
    out << machineNationality;
    out << cdromVector;
    out << pendingInterrupts;
    out << remainingCyclesThisFrame;
    out << z80TimeSlice;
    out << z80Disable;
    out << z80NMIDisable;
    out << currentTimeSeconds;
    out << audioCommand;
    out << audioResult;
    out << biosType;

    // M68K
    out << m68ki_cpu;

    // Z80
    out << Z80;

    // Timers
    out << timers;

    // Memory
    out << memory;

    // Video
    out << video;

    // Audio
    out << audio;

    // YM2610
    YM2610SaveState(out);

    // LC8951
    out << lc8951;

    // CDROM
    out << cdrom;

    return !out.fail();
}

bool NeoGeoCD::restoreState(DataPacker& in)
{
    // General machine state
    in >> cdzIrq1Divisor;
    in >> cdCommunicationNReset;
    in >> irqMask1;
    in >> irqMask2;
    in >> cdSectorDecodedThisFrame;
    in >> fastForward;
    in >> machineNationality;
    in >> cdromVector;
    in >> pendingInterrupts;
    in >> remainingCyclesThisFrame;
    in >> z80TimeSlice;
    in >> z80Disable;
    in >> z80NMIDisable;
    in >> currentTimeSeconds;
    in >> audioCommand;
    in >> audioResult;
    in >> biosType;

    // M68K
    in >> m68ki_cpu;
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    // The whole core struct is restored from the state, including its host
    // callback pointers. m68k_set_cpu_type above already restores the two
    // cycle-table pointers; clear every callback so a stale or hostile state
    // can never leave one pointing at an address of its choosing. The four
    // in the middle are unused in this build's config, but are cleared too
    // so enabling one later cannot turn a restore into an arbitrary call.
    m68ki_cpu.int_ack_callback = nullptr;
    m68ki_cpu.bkpt_ack_callback = nullptr;
    m68ki_cpu.reset_instr_callback = nullptr;
    m68ki_cpu.cmpild_instr_callback = nullptr;
    m68ki_cpu.rte_instr_callback = nullptr;
    m68ki_cpu.tas_instr_callback = nullptr;
    m68ki_cpu.illg_instr_callback = nullptr;
    m68ki_cpu.pc_changed_callback = nullptr;
    m68ki_cpu.set_fc_callback = nullptr;
    m68ki_cpu.instr_hook_callback = nullptr;

    // Z80
    in >> Z80;
    Z80.daisy = nullptr;
    Z80.irq_callback = z80_irq_callback;

    // Timers
    in >> timers;

    // Memory
    in >> memory;

    // Video
    in >> video;

    // Audio
    in >> audio;

    // YM2610
    YM2610RestoreState(in);

    // LC8951
    in >> lc8951;

    // CDROM
    in >> cdrom;

    return (!in.fail());
}

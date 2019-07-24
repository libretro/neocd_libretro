#include "neogeocd.h"
extern "C" {
    #include "3rdparty/musashi/m68kcpu.h"
}
#include "m68kintf.h"
#include "3rdparty/z80/z80.h"
#include "z80intf.h"
#include "3rdparty/ym/ym2610.h"
#include "timeprofiler.h"
#include "timer.h"

#include <algorithm>

NeoGeoCD neocd;

extern void cdromIRQTimerCallback(Timer* timer, uint32_t userData);
extern void cdromIRQ1TimerCDZCallback(Timer* timer, uint32_t userData);

NeoGeoCD::NeoGeoCD() :
    memory(),
    video(),
    cdrom(),
    lc8951(),
    timers(),
    input(),
    audio(),
    cdzIrq1Divisor(0),
    irqMasterEnable(false),
    irqMask1(0),
    irqMask2(0),
    irq1EnabledThisFrame(false),
    fastForward(false),
    machineNationality(NationalityJapan),
    cdromVector(0),
    pendingInterrupts(0),
    remainingCyclesThisFrame(0),
    m68kCyclesThisFrame(0),
    z80CyclesThisFrame(0),
    z80TimeSlice(0),
    z80Disable(true),
    z80NMIDisable(true),
    currentTimeSeconds(0.0),
    audioCommand(0),
    audioResult(0),
    biosType(FrontLoader)
{
}

void NeoGeoCD::initialize()
{
    // Initialize the 68000 emulation core
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    m68k_init();

    // Inizialize the z80 core
    z80_init(0, Timer::Z80_CLOCK, NULL, z80_irq_callback);

    // Initialize the YM2610
    YM2610Init(8000000, Audio::SAMPLE_RATE, neocd.memory.pcmRam, Memory::PCMRAM_SIZE, YM2610TimerHandler, YM2610IrqHandler);

    // Create the worker thread to buffer & decode audio data
    neocd.cdrom.createWorkerThread();
}

void NeoGeoCD::deinitialize()
{
    // End the worker thread
    neocd.cdrom.endWorkerThread();
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
    irqMasterEnable = false;
    cdzIrq1Divisor = 0;
    pendingInterrupts = 0;
    irqMask1 = 0;
    irqMask2 = 0;
    remainingCyclesThisFrame = 0;
    m68kCyclesThisFrame = 0;
    z80CyclesThisFrame = 0;
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
    PROFILE(p_total, ProfilingCategory::Total);

    remainingCyclesThisFrame += Timer::CYCLES_PER_FRAME;

    PROFILE(p_audioCD, ProfilingCategory::AudioCD);
    audio.initFrame();
    PROFILE_END(p_audioCD);

    while (remainingCyclesThisFrame > 0)
    {
        uint32_t timeSlice = std::min(neocd.timers.timeSlice(), remainingCyclesThisFrame);

        PROFILE(p_m68k, ProfilingCategory::CpuM68K);
        uint32_t elapsed = Timer::m68kToMaster(m68k_execute(Timer::masterToM68k(timeSlice)));
        PROFILE_END(p_m68k);

        m68kCyclesThisFrame += elapsed;

        z80TimeSlice += elapsed;
        if (z80TimeSlice > 0)
        {
            uint32_t z80Elapsed;

            if (z80Disable)
                z80Elapsed = z80TimeSlice;
            else
            {
                PROFILE(p_z80, ProfilingCategory::CpuZ80);
                z80Elapsed = Timer::z80ToMaster(z80_execute(Timer::masterToZ80(z80TimeSlice)));
            }

            z80CyclesThisFrame += z80Elapsed;
            z80TimeSlice -= z80Elapsed;
        }

        remainingCyclesThisFrame -= elapsed;

        audio.updateCurrentSample();

        currentTimeSeconds += Timer::masterToSeconds(elapsed);

        PROFILE(p_videoIRQ, ProfilingCategory::VideoAndIRQ);
        neocd.timers.advanceTime(elapsed);
        PROFILE_END(p_videoIRQ);
    }

    PROFILE(p_audioYM2610, ProfilingCategory::AudioYM2610);
    audio.finalize();
    PROFILE_END(p_audioYM2610);

    m68kCyclesThisFrame -= Timer::CYCLES_PER_FRAME;
    z80CyclesThisFrame -= Timer::CYCLES_PER_FRAME;
}

void NeoGeoCD::setInterrupt(NeoGeoCD::Interrupt interrupt)
{
    pendingInterrupts |= interrupt;
}

void NeoGeoCD::clearInterrupt(NeoGeoCD::Interrupt interrupt)
{
    pendingInterrupts &= ~interrupt;
}

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

int NeoGeoCD::getScreenX() const
{
    return (Timer::masterToPixel(Timer::CYCLES_PER_FRAME - remainingCyclesThisFrame) % Timer::SCREEN_WIDTH);
}

int NeoGeoCD::getScreenY() const
{
    return (Timer::masterToPixel(Timer::CYCLES_PER_FRAME - remainingCyclesThisFrame) / Timer::SCREEN_WIDTH);
}

bool NeoGeoCD::saveState(DataPacker& out) const
{
    // General machine state
    out << cdzIrq1Divisor;
    out << irqMasterEnable;
    out << irqMask1;
    out << irqMask2;
    out << irq1EnabledThisFrame;
    out << fastForward;
    out << machineNationality;
    out << cdromVector;
    out << pendingInterrupts;
    out << remainingCyclesThisFrame;
    out << m68kCyclesThisFrame;
    out << z80CyclesThisFrame;
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
    out << neocd.timers;

    // Memory
    out << neocd.memory;

    // Video
    out << neocd.video;

    // Audio
    out << neocd.audio;

    // YM2610
    YM2610SaveState(out);

    // LC8951
    out << neocd.lc8951;

    // CDROM
    out << neocd.cdrom;

    return !out.fail();
}

bool NeoGeoCD::restoreState(DataPacker& in)
{
    // General machine state
    in >> cdzIrq1Divisor;
    in >> irqMasterEnable;
    in >> irqMask1;
    in >> irqMask2;
    in >> irq1EnabledThisFrame;
    in >> fastForward;
    in >> machineNationality;
    in >> cdromVector;
    in >> pendingInterrupts;
    in >> remainingCyclesThisFrame;
    in >> m68kCyclesThisFrame;
    in >> z80CyclesThisFrame;
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
    m68ki_cpu.int_ack_callback = nullptr;
    m68ki_cpu.bkpt_ack_callback = nullptr;
    m68ki_cpu.reset_instr_callback = nullptr;
    m68ki_cpu.pc_changed_callback = nullptr;
    m68ki_cpu.set_fc_callback = nullptr;
    m68ki_cpu.instr_hook_callback = nullptr;

    // Z80
    in >> Z80;
    Z80.daisy = nullptr;
    Z80.irq_callback = z80_irq_callback;

    // Timers
    in >> neocd.timers;

    // Memory
    in >> neocd.memory;

    // Video
    in >> neocd.video;

    // Audio
    in >> neocd.audio;

    // YM2610
    YM2610RestoreState(in);

    // LC8951
    in >> neocd.lc8951;

    // CDROM
    in >> neocd.cdrom;

    return (!in.fail());
}

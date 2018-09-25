#include "timergroup.h"
#include "neogeocd.h"
#include "3rdparty/musashi/m68kcpu.h"
#include "3rdparty/z80/z80.h"
#include "3rdparty/ym/ym2610.h"
#include "round.h"

#include <algorithm>

void watchdogTimerCallback(Timer* timer, uint32_t userData)
{
    LOG(LOG_ERROR, "WARNING: Watchdog timer triggered (PC=%06X, SR=%04X); Machine reset.\n",
        m68k_get_reg(NULL, M68K_REG_PPC),
        m68k_get_reg(NULL, M68K_REG_SR));

    m68k_pulse_reset();
}

void vblTimerCallback(Timer* timer, uint32_t userData)
{
    // Handle HIRQ_CTRL_VBLANK_LOAD
    if (neocd.video.hirqControl & Video::HIRQ_CTRL_VBLANK_LOAD)
        neocd.timers.hirqTimer->arm(timer->delay() + Timer::pixelToMaster(neocd.video.hirqRegister + 1));

    // Set VBL IRQ to run
    if (neocd.isVBLEnabled())
    {
        neocd.setInterrupt(NeoGeoCD::VerticalBlank);
        neocd.updateInterrupts();
    }

    // Update auto animation counter
    if (!neocd.video.autoAnimationFrameCounter)
    {
        neocd.video.autoAnimationFrameCounter = neocd.video.autoAnimationSpeed;
        neocd.video.autoAnimationCounter++;
    }
    else
        neocd.video.autoAnimationFrameCounter--;

    // Set next VBL
    timer->armRelative(Timer::pixelToMaster(Timer::SCREEN_WIDTH * Timer::SCREEN_HEIGHT));
}

void hirqTimerCallback(Timer* timer, uint32_t userData)
{
    // Trigger horizontal interrupt if enabled
    if (neocd.video.hirqControl & Video::HIRQ_CTRL_ENABLE)
    {
/*		LOG(LOG_INFO, "Horizontal IRQ.@ (%d,%d), next drawline in : %d\n",
            neocd.getScreenX(),
            neocd.getScreenY(),
            Timer::masterToPixel(neocd.timers.drawlineTimer->delay()));*/

        neocd.setInterrupt(NeoGeoCD::Raster);
        neocd.updateInterrupts();
    }

    // Neo Drift Out sets HIRQ_reg = 0xFFFFFFFF and auto-repeat, we need to check for it
    // I don't know the exact reason for the need to add 1 to hirqRegister + 1 here and several other parts of the code,
    // but it's important for line effects in Karnov's Revenge to work correctly.
    if ((neocd.video.hirqControl & Video::HIRQ_CTRL_AUTOREPEAT) && (neocd.video.hirqRegister != 0xFFFFFFFF))
        timer->armRelative(Timer::pixelToMaster(std::max(uint32_t(1), neocd.video.hirqRegister + 1)));
}

void ym2610TimerCallback(Timer* Timer, uint32_t data)
{
    YM2610TimerOver(data);
}

void drawlineTimerCallback(Timer* timer, uint32_t userData)
{
//	LOG(LOG_INFO, "Drawline.@ (%d,%d)\n", neocd.getScreenX(), neocd.getScreenY());

    uint32_t scanline = neocd.getScreenY();

    // Video content is generated between scanlines 16 and 240, see timer.h
    if ((scanline >= Timer::FIRST_LINE) && (scanline < Timer::VBLANK_LINE))
    {
        if (!neocd.fastForward)
        {
            if (neocd.video.videoEnable)
            {
                neocd.video.drawEmptyLine(scanline);

                if (!neocd.video.sprDisable)
                {
                    if (scanline & 1)
                    {
                        uint16_t activeSprites = neocd.video.createSpriteList(scanline, &neocd.memory.videoRam[0x8680]);
                        neocd.video.drawSprites(scanline, &neocd.memory.videoRam[0x8680], activeSprites);
                    }
                    else
                    {
                        uint16_t activeSprites = neocd.video.createSpriteList(scanline, &neocd.memory.videoRam[0x8600]);
                        neocd.video.drawSprites(scanline, &neocd.memory.videoRam[0x8600], activeSprites);
                    }
                }

                if (!neocd.video.fixDisable)
                    neocd.video.drawFix(scanline);
            }
            else
                neocd.video.drawBlackLine(scanline);
        }
    }

    timer->armRelative(Timer::pixelToMaster(Timer::SCREEN_WIDTH));
}

void cdromIRQTimerCallback(Timer* timer, uint32_t userData)
{
    timer->armRelative(neocd.isCDZ() ? (Timer::CDROM_DELAY / 2) : Timer::CDROM_DELAY);

    if (neocd.cdrom.isPlaying())
    {
        neocd.lc8951.sectorDecoded();
        
        // Trigger the CD IRQ1 if needed, this is used when reading data sectors
        if (neocd.isIRQ1Enabled() && (neocd.lc8951.IFCTRL & LC8951::DECIEN) && !(neocd.lc8951.IFSTAT & LC8951::DECI))
            neocd.setInterrupt(NeoGeoCD::CdromDecoder);
        
        if (neocd.cdrom.isData())
            neocd.cdzIrq1Divisor = 0;
        else if (neocd.isCDZ())
        {
            // If the machine is CDZ head position is updated every 2 interrupts for audio tracks
            neocd.cdzIrq1Divisor ^= 1;
        }

        if (!neocd.cdzIrq1Divisor)
        {
            // Update the read position
            neocd.cdrom.increasePosition();
        }
    }
    
    // Trigger the CDROM IRQ2, this is used to send commands packets / receive answer packets.
    if (neocd.isIRQ2Enabled())
        neocd.setInterrupt(NeoGeoCD::CdromCommunication);

    // Update interrupts
    neocd.updateInterrupts();
}

void audioCommandTimerCallback(Timer* timer, uint32_t userData)
{
    uint32_t z80Elapsed;

    // Post the audio command to the Z80
    neocd.audioCommand = userData;
    
    // If the NMI is not disabled
    if (!neocd.z80NMIDisable)
    {
        // Trigger it
        z80_set_irq_line(INPUT_LINE_NMI, ASSERT_LINE);
        z80_set_irq_line(INPUT_LINE_NMI, CLEAR_LINE);

        // Let the Z80 take the command into account
        z80Elapsed = Timer::z80ToMaster(z80_execute(Timer::masterToZ80(2000)));
        neocd.z80TimeSlice -= z80Elapsed;
        neocd.z80CyclesThisFrame += z80Elapsed;
    }
}

TimerGroup::TimerGroup() :
    watchdogTimer(nullptr),
    vblTimer(nullptr),
    hirqTimer(nullptr),
    drawlineTimer(nullptr),
    cdromIRQTimer(nullptr),
    ym2610TimerA(nullptr),
    ym2610TimerB(nullptr),
    audioCommandTimer(nullptr),
    m_timers()
{
    m_timers.push_back(Timer());
    m_timers.push_back(Timer());
    m_timers.push_back(Timer());
    m_timers.push_back(Timer());
    m_timers.push_back(Timer());
    m_timers.push_back(Timer());
    m_timers.push_back(Timer());
    m_timers.push_back(Timer());
    m_timers.push_back(Timer());

    int i = 0;

    watchdogTimer = &m_timers[i++];
    watchdogTimer->setDelay(Timer::WATCHDOG_DELAY);
    watchdogTimer->setCallback(watchdogTimerCallback);

    drawlineTimer = &m_timers[i++];
    drawlineTimer->setCallback(drawlineTimerCallback);

    vblTimer = &m_timers[i++];
    vblTimer->setCallback(vblTimerCallback);

    hirqTimer = &m_timers[i++];
    hirqTimer->setCallback(hirqTimerCallback);

    cdromIRQTimer = &m_timers[i++];
    cdromIRQTimer->setCallback(cdromIRQTimerCallback);

    audioCommandTimer = &m_timers[i++];
    audioCommandTimer->setCallback(audioCommandTimerCallback);

    ym2610TimerA = &m_timers[i++];
    ym2610TimerA->setUserData(0);
    ym2610TimerA->setCallback(ym2610TimerCallback);

    ym2610TimerB = &m_timers[i++];
    ym2610TimerB->setUserData(1);
    ym2610TimerB->setCallback(ym2610TimerCallback);
}

void TimerGroup::reset()
{
    watchdogTimer->setState(Timer::Stopped);

    drawlineTimer->arm(Timer::pixelToMaster(Timer::HS_START));
    vblTimer->arm(Timer::pixelToMaster(Timer::SCREEN_WIDTH * Timer::VBLANK_LINE));

    cdromIRQTimer->arm(neocd.isCDZ() ? (Timer::CDROM_DELAY / 2) : Timer::CDROM_DELAY);

    hirqTimer->setState(Timer::Stopped);

    audioCommandTimer->setState(Timer::Stopped);

    ym2610TimerA->setState(Timer::Stopped);
    ym2610TimerB->setState(Timer::Stopped);
}

int32_t TimerGroup::timeSlice() const
{
    int32_t timeSlice = round<int32_t>(Timer::MASTER_CLOCK / Timer::FRAME_RATE);

    for(const Timer& timer : m_timers)
    {
        if (timer.isActive())
            timeSlice = std::min(timeSlice, timer.delay());
    }

    return timeSlice;
}

void TimerGroup::advanceTime(const int32_t time)
{
    for(Timer& timer : m_timers)
        timer.advanceTime(time);
}

DataPacker& operator<<(DataPacker& out, const TimerGroup& timerGroup)
{
    out << *timerGroup.watchdogTimer;
    out << *timerGroup.vblTimer;
    out << *timerGroup.hirqTimer;
    out << *timerGroup.drawlineTimer;
    out << *timerGroup.cdromIRQTimer;
    out << *timerGroup.ym2610TimerA;
    out << *timerGroup.ym2610TimerB;
    out << *timerGroup.audioCommandTimer;

    return out;
}

DataPacker& operator>>(DataPacker& in, TimerGroup& timerGroup)
{
    in >> *timerGroup.watchdogTimer;
    in >> *timerGroup.vblTimer;
    in >> *timerGroup.hirqTimer;
    in >> *timerGroup.drawlineTimer;
    in >> *timerGroup.cdromIRQTimer;
    in >> *timerGroup.ym2610TimerA;
    in >> *timerGroup.ym2610TimerB;
    in >> *timerGroup.audioCommandTimer;

    return in;
}

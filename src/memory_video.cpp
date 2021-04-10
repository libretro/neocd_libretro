#include "memory_video.h"
extern "C" {
    #include "3rdparty/musashi/m68kcpu.h"
}
#include "neogeocd.h"

#include <algorithm>

static uint32_t videoRamReadWord(uint32_t address)
{
    uint32_t verticalPosition;

    switch (address)
    {
    case    0x0:    // Videoram Data
    case    0x2:
        return  neocd.video.videoramData;

    case    0x4:    // Videoram Modulo
        return  neocd.video.videoramModulo;

    case    0x6:    // Auto animation speed & H IRQ control
        verticalPosition = neocd.getScreenY() + 0x100;
        if (verticalPosition >= 0x200)
            verticalPosition = verticalPosition - Timer::SCREEN_HEIGHT;

        return ((verticalPosition << 7) | (neocd.video.autoAnimationCounter & 7));
    }

    return 0xFFFF;
}

static void videoRamWriteWord(uint32_t address, uint32_t data)
{
    switch (address)
    {
    case    0x0:    // $3C0000: Videoram Offset
        neocd.video.videoramOffset = data;
        neocd.video.videoramData = neocd.memory.videoRam[neocd.video.videoramOffset];
        break;
    case    0x2:    // $3C0002: Videoram Data
        neocd.memory.videoRam[neocd.video.videoramOffset] = data;
        neocd.video.videoramOffset = (neocd.video.videoramOffset & 0x8000) | ((neocd.video.videoramOffset + neocd.video.videoramModulo) & 0x7FFF);
        neocd.video.videoramData = neocd.memory.videoRam[neocd.video.videoramOffset];
        break;

    case    0x4:    // $3C0004: Videoram Modulo
        neocd.video.videoramModulo = data;
        break;

    case    0x6:    // $3C0006: Auto animation speed & H IRQ control
        neocd.video.autoAnimationSpeed = data >> 8;
        neocd.video.autoAnimationDisabled = (data & 0x0008) != 0;
        neocd.video.hirqControl = data & 0x00F0;
        break;

    case    0x8:    // $3C0008: Display counter high
        neocd.video.hirqRegister = (neocd.video.hirqRegister & 0x0000FFFF) | (data << 16);
        break;

    case    0xA:    // $3C000A: Display Counter low
        neocd.video.hirqRegister = (neocd.video.hirqRegister & 0xFFFF0000) | data;
        if (neocd.video.hirqControl & Video::HIRQ_CTRL_RELATIVE)
        {
            // Karnov uses this for raster effects, they calculate precisely the number of cycles to wait for the next line.
            // We need to take into account the number of cycles already executed in the timeslice.
            // A<--->t<----------->B (A=Current time for the emulator, t=time in the m68k timeslice, B=Set time for HIRQ)
            // A<--hirqReg-->      B Too early.
            // A<--t-><--hirqReg-->B Correct.
            uint32_t timesliceElapsed = Timer::m68kToMaster(m68k_cycles_run());
            uint32_t delay = Timer::pixelToMaster(neocd.video.hirqRegister + 1);
            neocd.timers.timer<TimerGroup::Hbl>().arm(timesliceElapsed + delay);
        }
        break;
    case    0xC:    // $3C000C: IRQ Acknowledge
        if (data & 0x02)
            neocd.clearInterrupt(NeoGeoCD::Raster);
        if (data & 0x04)
            neocd.clearInterrupt(NeoGeoCD::VerticalBlank);

        neocd.updateInterrupts();
        break;

    case    0xE:    /* $3C000E: Unknown */
        LOG(LOG_INFO, "VIDEO: Write to register $3C000E (Data=%04X)\n", data);
        break;
    }
}

static uint32_t videoRamReadByte(uint32_t address)
{
    if (!(address & 1))
        return videoRamReadWord(address & 6) >> 8;

    return 0xFF;
}

static void videoRamWriteByte(uint32_t address, uint32_t data)
{
    if (!(address & 1))
    {
        data = (data << 8) | data;
        videoRamWriteWord(address, data);
    }
}

const Memory::Handlers videoRamHandlers = {
    videoRamReadByte,
    videoRamReadWord,
    videoRamWriteByte,
    videoRamWriteWord
};

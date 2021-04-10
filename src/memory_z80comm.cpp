#include "memory_z80comm.h"
extern "C" {
    #include "3rdparty/musashi/m68kcpu.h"
}
#include "neogeocd.h"

static uint32_t z80CommunicationReadByte(uint32_t address)
{
    if (!address)
        return neocd.audioResult;

    return 0xFF;
}

static uint32_t z80CommunicationReadWord(uint32_t address)
{
    return (neocd.audioResult << 8) | 0xFF;
}

static void z80CommunicationWriteByte(uint32_t address, uint32_t data)
{
    if (!address)
    {
        neocd.timers.timer<TimerGroup::AudioCommand>().setUserData(data);

        // 1 here, not zero otherwise the callback would be called immediately
        neocd.timers.timer<TimerGroup::AudioCommand>().arm(1);

        // End the 68K timeslice here so the Z80 can run up to the current point, then the pseudo audio command timer will trigger the IRQ
        m68k_end_timeslice();
    }
}

static void z80CommunicationWriteWord(uint32_t address, uint32_t data)
{
    neocd.timers.timer<TimerGroup::AudioCommand>().setUserData(data >> 8);

    // 1 here, not zero otherwise the callback would be called immediately
    neocd.timers.timer<TimerGroup::AudioCommand>().arm(1);

    // End the 68K timeslice here so the Z80 can run up to the current point, then the pseudo audio command timer will trigger the IRQ
    m68k_end_timeslice();
}

const Memory::Handlers z80CommunicationHandlers = {
    z80CommunicationReadByte,
    z80CommunicationReadWord,
    z80CommunicationWriteByte,
    z80CommunicationWriteWord
};

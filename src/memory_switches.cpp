#include "memory_switches.h"
#include "3rdparty/musashi/m68kcpu.h"
#include "neogeocd.h"

static uint32_t switchReadByte(uint32_t address)
{
    return 0xFF;
}

static uint32_t switchReadWord(uint32_t address)
{
    return 0xFFFF;
}

/*
    The vector area affected by the switch is 0x80 bytes.
    When ROM is mapped to address 0, writing to the area has no effect.
*/
static void switchWriteWord(uint32_t address, uint32_t data)
{
    switch (address)
    {
    case 0x00:  // Darken colors, ignored for now 
    case 0x10:
        break;

    case 0x02:  // Set ROM vectors
        neocd.memory.mapVectorsToRom();
        break;

    case 0x0e:  // Set Palette bank 0
        neocd.video.activePaletteBank = 0;
        break;

    case 0x12:  // Set RAM vectors
        neocd.memory.mapVectorsToRam();
        break;

    case 0x1e:  // Set palette bank 1
        neocd.video.activePaletteBank = 1;
        break;

    default:    // unknown
        LOG(LOG_INFO, "SWITCHES: Write to unknown switch %06X @ PC=%06X DATA=%04X\n", address + 0x3A0000, m68k_get_reg(NULL, M68K_REG_PPC), data);
        break;
    }
}

static void switchWriteByte(uint32_t address, uint32_t data)
{
    if (address & 1)
    {
        address &= 0xFFFFFE;
        switchWriteWord(address, data);
    }
}

const Memory::Handlers switchHandlers = {
    switchReadByte,
    switchReadWord,
    switchWriteByte,
    switchWriteWord
};

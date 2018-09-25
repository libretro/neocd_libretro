#include "memory_paletteram.h"
#include "neogeocd.h"

static uint32_t paletteRamReadByte(uint32_t address)
{
    return *(reinterpret_cast<uint8_t*>(&neocd.memory.paletteRam[neocd.video.activePaletteBank * 4096]) + address);
}

static uint32_t paletteRamReadWord(uint32_t address)
{
    return BIG_ENDIAN_WORD(neocd.memory.paletteRam[(neocd.video.activePaletteBank * 4096) + (address / 2)]);
}

static void paletteRamWriteByte(uint32_t address, uint32_t data)
{
    uint32_t color = (neocd.video.activePaletteBank * 4096) + (address / 2);
    *(reinterpret_cast<uint8_t*>(&neocd.memory.paletteRam[neocd.video.activePaletteBank * 4096]) + address) = data;
    neocd.video.convertColor(color);
}

static void paletteRamWriteWord(uint32_t address, uint32_t data)
{
    uint32_t color = (neocd.video.activePaletteBank * 4096) + (address / 2);
    neocd.memory.paletteRam[color] = BIG_ENDIAN_WORD(data);
    neocd.video.convertColor(color);
}

const Memory::Handlers paletteRamHandlers = {
    paletteRamReadByte,
    paletteRamReadWord,
    paletteRamWriteByte,
    paletteRamWriteWord
};

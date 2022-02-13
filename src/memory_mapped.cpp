#include "libretro_common.h"
#include "memory_mapped.h"
#include "neogeocd.h"

static uint32_t mappedRamReadByte(uint32_t address)
{
    if (neocd->memory.areaSelect & neocd->memory.busRequest)
    {
        switch (neocd->memory.areaSelect)
        {
        case Memory::AREA_FIX:
            if (address & 1)
            {
                address = ((address >> 1) & 0x1FFFF);
                return neocd->memory.fixRam[address];
            }
            break;

        case Memory::AREA_SPR:
            address += ((neocd->memory.sprBankSelect & 3) * 0x100000);
            address &= 0x3FFFFF;
            return neocd->memory.sprRam[address];
            break;

        case Memory::AREA_Z80:
            if (address & 1)
            {
                address = ((address >> 1) & 0xFFFF);
                return neocd->memory.z80Ram[address];
            }
            break;

        case Memory::AREA_PCM:
            if (address & 1)
            {
                address = ((address >> 1) + ((neocd->memory.pcmBankSelect & 1) * 0x80000)) & 0xFFFFF;
                return neocd->memory.pcmRam[address];
            }
            break;
        }
    }

    return 0xFF;
}

static uint32_t mappedRamReadWord(uint32_t address)
{
    if (neocd->memory.areaSelect & neocd->memory.busRequest)
    {
        uint16_t    *wordPtr;

        switch (neocd->memory.areaSelect)
        {
        case Memory::AREA_FIX:
            address = ((address >> 1) & 0x1FFFF);
            return (neocd->memory.fixRam[address] | 0xFF00);
            break;

        case Memory::AREA_SPR:
            address += ((neocd->memory.sprBankSelect & 3) * 0x100000);
            address &= 0x3FFFFE;
            wordPtr = (uint16_t*)&neocd->memory.sprRam[address];
            return BIG_ENDIAN_WORD(*wordPtr);
            break;

        case Memory::AREA_Z80:
            address = ((address >> 1) & 0xFFFF);
            return (neocd->memory.z80Ram[address] | 0xFF00);
            break;

        case Memory::AREA_PCM:
            address = ((address >> 1) + ((neocd->memory.pcmBankSelect & 1) * 0x80000)) & 0xFFFFF;
            return (neocd->memory.pcmRam[address] | 0xFF00);
            break;
        }
    }

    return 0xFFFF;
}

static void mappedRamWriteByte(uint32_t address, uint32_t data)
{
    if (neocd->memory.areaSelect & neocd->memory.busRequest)
    {
        switch (neocd->memory.areaSelect)
        {
        case Memory::AREA_FIX:
            if (address & 1)
            {
                address = ((address >> 1) & 0x1FFFF);
                neocd->memory.fixRam[address] = data;
            }
            break;

        case Memory::AREA_SPR:
            address += ((neocd->memory.sprBankSelect & 3) * 0x100000);
            address &= 0x3FFFFF;
            neocd->memory.sprRam[address] = data;
            break;

        case Memory::AREA_Z80:
            if (address & 1)
            {
                address = ((address >> 1) & 0xFFFF);
                neocd->memory.z80Ram[address] = data;
            }
            break;

        case Memory::AREA_PCM:
            if (address & 1)
            {
                address = ((address >> 1) + ((neocd->memory.pcmBankSelect & 1) * 0x80000)) & 0xFFFFF;
                neocd->memory.pcmRam[address] = data;
            }
            break;
        }
    }
}

static void mappedRamWriteWord(uint32_t address, uint32_t data)
{
    if (neocd->memory.areaSelect & neocd->memory.busRequest)
    {
        uint16_t *wordPtr;

        switch (neocd->memory.areaSelect)
        {
        case Memory::AREA_FIX:
            address = ((address >> 1) & 0x1FFFF);
            neocd->memory.fixRam[address] = data;
            break;

        case Memory::AREA_SPR:
            address += ((neocd->memory.sprBankSelect & 3) * 0x100000);
            address &= 0x3FFFFE;
            wordPtr = (uint16_t*)&neocd->memory.sprRam[address];
            *wordPtr = BIG_ENDIAN_WORD(data);
            break;

        case Memory::AREA_Z80:
            address = ((address >> 1) & 0xFFFF);
            neocd->memory.z80Ram[address] = data;
            break;

        case Memory::AREA_PCM:
            address = ((address >> 1) + ((neocd->memory.pcmBankSelect & 1) * 0x80000)) & 0xFFFFF;
            neocd->memory.pcmRam[address] = data;
            break;
        }
    }
}

const Memory::Handlers mappedRamHandlers = {
    mappedRamReadByte,
    mappedRamReadWord,
    mappedRamWriteByte,
    mappedRamWriteWord
};

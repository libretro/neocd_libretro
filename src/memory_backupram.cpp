#include "memory_backupram.h"
#include "neogeocd.h"

static uint32_t backupRamReadByte(uint32_t address)
{
    if (address & 1)
        return neocd.memory.backupRam[address >> 1];

    return 0xFF;
}

static uint32_t backupRamReadWord(uint32_t address)
{
    return (neocd.memory.backupRam[address >> 1] | 0xFF00);
}

static void backupRamWriteByte(uint32_t address, uint32_t data)
{
    if (address & 1)
        neocd.memory.backupRam[address >> 1] = data;
}

static void backupRamWriteWord(uint32_t address, uint32_t data)
{
    neocd.memory.backupRam[address >> 1] = data;
}

const Memory::Handlers backupRamHandlers = {
    backupRamReadByte,
    backupRamReadWord,
    backupRamWriteByte,
    backupRamWriteWord
};

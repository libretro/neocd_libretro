#include "m68kintf.h"
#include "neogeocd.h"

extern "C" {

    #include "3rdparty/musashi/m68kcpu.h"

    void m68ki_exception_bus_error(void)
    {
        LOG(LOG_ERROR, "Bus Error @ PC=%X.", REG_PPC);

        uint_fast32_t sr = m68ki_init_exception();

        /* If we were processing a bus error, address error, or reset,
        * this is a catastrophic failure.
        * Halt the CPU
        */
        if (CPU_RUN_MODE == RUN_MODE_BERR_AERR_RESET)
        {
//          m68k_read_memory_8(0x00ffff01);
            CPU_STOPPED = STOP_LEVEL_HALT;
            return;
        }
        CPU_RUN_MODE = RUN_MODE_BERR_AERR_RESET;

        /* Note: This is implemented for 68000 only! */
        m68ki_stack_frame_buserr(sr);

        m68ki_jump_vector(EXCEPTION_BUS_ERROR);

        /* Use up some clock cycles and undo the instruction's cycles */
        USE_CYCLES(CYC_EXCEPTION[EXCEPTION_BUS_ERROR] - CYC_INSTRUCTION[REG_IR]);

        SET_CYCLES(CYC_INSTRUCTION[REG_IR]);
    }

    uint32_t m68k_read_memory_8(uint32_t address)
    {
        const Memory::Region* region = neocd.memory.regionLookupTable[address / Memory::MEMORY_GRANULARITY];

        if (!region)
        {
            m68ki_exception_bus_error();
            return 0xFF;
        }

        if (region->flags & Memory::Region::ReadDirect)
            return region->readBase[address & region->addressMask];

        if (region->flags & Memory::Region::ReadMapped)
            return region->handlers->readByte(address & region->addressMask);

        // Memory::Region::ReadNop
        return 0xFF;
    }

    void m68k_write_memory_8(uint32_t address, uint32_t data)
    {
        const Memory::Region* region = neocd.memory.regionLookupTable[address / Memory::MEMORY_GRANULARITY];

        if (!region)
        {
            m68ki_exception_bus_error();
            return;
        }

        if (region->flags & Memory::Region::WriteDirect)
        {
            region->writeBase[address & region->addressMask] = data;
            return;
        }

        if (region->flags & Memory::Region::WriteMapped)
        {
            region->handlers->writeByte(address & region->addressMask, data);
            return;
        }

        // Memory::Region::WriteNop
    }

    uint32_t m68k_read_memory_16(uint32_t address)
    {
        const Memory::Region* region = neocd.memory.regionLookupTable[address / Memory::MEMORY_GRANULARITY];

        if (!region)
        {
            m68ki_exception_bus_error();
            return 0xFFFF;
        }

        if (region->flags & Memory::Region::ReadDirect)
            return BIG_ENDIAN_WORD(*reinterpret_cast<const uint16_t*>(&region->readBase[address  & region->addressMask]));

        if (region->flags & Memory::Region::ReadMapped)
            return region->handlers->readWord(address & region->addressMask);

        // Memory::Region::ReadNop
        return 0xFFFF;
    }

    void m68k_write_memory_16(uint32_t address, uint32_t data)
    {
        const Memory::Region* region = neocd.memory.regionLookupTable[address / Memory::MEMORY_GRANULARITY];

        if (!region)
        {
            m68ki_exception_bus_error();
            return;
        }

        if (region->flags & Memory::Region::WriteDirect)
        {
            *reinterpret_cast<uint16_t*>(&region->writeBase[address & region->addressMask]) = BIG_ENDIAN_WORD(data);
            return;
        }

        if (region->flags & Memory::Region::WriteMapped)
        {
            region->handlers->writeWord(address & region->addressMask, data);
            return;
        }

        // Memory::Region::WriteNop
    }

    uint32_t m68k_read_memory_32(uint32_t address)
    {
        return (m68k_read_memory_16(address) << 16) | m68k_read_memory_16(address + 2);
    }

    void m68k_write_memory_32(uint32_t address, uint32_t data)
    {
        m68k_write_memory_16(address, data >> 16);
        m68k_write_memory_16(address + 2, data & 0xFFFF);
    }

    /*
        Interrupt levels have been checked by hooking the interrupts
        and writing SR somewhere in memory, which reveals the interrupt level.

        VBL IRQ ($68):           SR = 2100
        CD-ROM IRQs ($54 & $58): SR = 2200
        SCANLINE IRQ ($64):      SR = 2300
    */
    int neocd_get_vector(int level)
    {
        int vector = M68K_INT_ACK_AUTOVECTOR;

        switch (level)
        {
        case 1:
            vector = 0x68 / 4;
            break;
        case 2:
            vector = neocd.cdromVector / 4;
            break;
        case 3:
            vector = 0x64 / 4;
            break;
        }

        return vector;
    }
}

#include "3rdparty/ym/ym2610.h"
#include "libretro_common.h"
#include "neogeocd.h"
#include "z80intf.h"

extern "C"
{
    uint16_t io_read_byte_8(uint16_t port)
    {
        switch (port & 0xFF)
        {
        case 0x00:  // Sound code
            return neocd->audioCommand;

        case 0x04:  // Status port A
            return YM2610Read(0);

        case 0x05:  // Read port A
            return YM2610Read(1);

        case 0x06:  // Status port B
            return YM2610Read(2);

        case 0x07: // Read port B
            return YM2610Read(3);
        }

        return 0;
    }

    void io_write_byte_8(uint16_t port, uint16_t value)
    {
        switch (port & 0xFF)
        {
        case 0x00: // Clear sound code
            neocd->audioCommand = 0;
            break;

        case 0x04:  // Control port A
            YM2610Write(0, (uint8_t)value);
            break;

        case 0x05:  // Data port A
            YM2610Write(1, (uint8_t)value);
            break;

        case 0x06:  // Control port B
            YM2610Write(2, (uint8_t)value);
            break;

        case 0x07:  // Data port B
            YM2610Write(3, (uint8_t)value);
            break;

        case 0x08: // NMI Enable
            neocd->z80NMIDisable = false;
            break;

        case 0x0C:   // Set audio result
            neocd->audioResult = value;
            break;

        case 0x18: // NMI Disable
            neocd->z80NMIDisable = true;
            break;
        }
    }

    uint8_t program_read_byte_8(uint16_t addr)
    {
        return neocd->memory.z80Ram[addr];
    }

    void program_write_byte_8(uint16_t addr, uint8_t value)
    {
        neocd->memory.z80Ram[addr] = value;
    }

    int z80_irq_callback(int parameter)
    {
        return 0x38;
    }
}

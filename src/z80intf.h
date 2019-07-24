#ifndef Z80INTF_H
#define Z80INTF_H

#include <stdint.h>

#include "inline.h"

#define Uint8 uint8_t
#define Sint8 int8_t
#define Uint16 uint16_t
#define Uint32 uint32_t

#ifdef __cplusplus
extern "C" {
#endif

uint16_t io_read_byte_8(uint16_t port);

void io_write_byte_8(uint16_t port, uint16_t value);

uint8_t program_read_byte_8(uint16_t addr);

void program_write_byte_8(uint16_t addr, uint8_t value);

int z80_irq_callback(int parameter);

#ifdef __cplusplus
}
#endif

#endif // Z80INTF_H

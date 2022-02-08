#ifndef M68KINTF_H
#define M68KINTF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t m68k_read_memory_8(uint32_t address);
void m68k_write_memory_8(uint32_t address, uint32_t data);
uint32_t m68k_read_memory_16(uint32_t address);
void m68k_write_memory_16(uint32_t address, uint32_t data);
uint32_t m68k_read_memory_32(uint32_t address);
void m68k_write_memory_32(uint32_t address, uint32_t data);

int neocd_get_vector(int level);

int neocd_illegal_handler(int instruction);

#ifdef __cplusplus
}
#endif

#endif // M68KINTF_H

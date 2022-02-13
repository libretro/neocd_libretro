#include <array>

#include "libretro_common.h"
#include "libretro_memmap.h"
#include "neogeocd.h"

enum RetroMapIndex
{
    RAM,
    ROM,
    VRAM,
    Z80,
    BKCP,
    COUNT
};

// Memory descriptors for cheats, achievements, etc...
static std::array<retro_memory_descriptor, RetroMapIndex::COUNT> memoryDescriptors;

// Memory map for cheats, achievements, etc...
static retro_memory_map memoryMap;

void Libretro::Memmap::init()
{
    memoryDescriptors[RetroMapIndex::RAM]  = { RETRO_MEMDESC_SYSTEM_RAM | RETRO_MEMDESC_BIGENDIAN, reinterpret_cast<void*>(neocd->memory.ram),       0, 0x00000000, 0, 0, Memory::RAM_SIZE,       "RAM" };
    memoryDescriptors[RetroMapIndex::ROM]  = { RETRO_MEMDESC_CONST | RETRO_MEMDESC_BIGENDIAN,      reinterpret_cast<void*>(neocd->memory.rom),       0, 0x00C00000, 0, 0, Memory::ROM_SIZE,       "ROM" };

    // Virtual addresses

    memoryDescriptors[RetroMapIndex::VRAM] = { RETRO_MEMDESC_VIDEO_RAM,                            reinterpret_cast<void*>(neocd->memory.videoRam),  0, 0x10000000, 0, 0, Memory::VIDEORAM_SIZE,  "VRAM" };
    memoryDescriptors[RetroMapIndex::Z80]  = { 0,                                                  reinterpret_cast<void*>(neocd->memory.z80Ram),    0, 0x20000000, 0, 0, Memory::Z80RAM_SIZE,    "Z80" };
    memoryDescriptors[RetroMapIndex::BKCP] = { RETRO_MEMDESC_SAVE_RAM,                             reinterpret_cast<void*>(neocd->memory.backupRam), 0, 0x30000000, 0, 0, Memory::BACKUPRAM_SIZE, "BKCP" };

    memoryMap.num_descriptors = RetroMapIndex::COUNT;
    memoryMap.descriptors = memoryDescriptors.data();

    libretro.environment(RETRO_ENVIRONMENT_SET_MEMORY_MAPS, &memoryMap);
}

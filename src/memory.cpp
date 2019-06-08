#include "neogeocd.h"
#include "memory.h"
#include "memory_paletteram.h"
#include "memory_cdintf.h"
#include "memory_input.h"
#include "memory_mapped.h"
#include "memory_z80comm.h"
#include "memory_backupram.h"
#include "memory_video.h"
#include "memory_switches.h"
extern "C" {
    #include "3rdparty/musashi/m68kcpu.h"
}

#include <cstring>
#include <algorithm>

/*
    What you see on a real Neo Geo CD:

    START       END         DESCRIPTION
    0x000000    0x00007F    ROM vectors or RAM
    0x000080    0x1FFFFF    RAM
    0x200000    0x2FFFFF    Random data. Whatever is on the data bus?
    0x300000    0x31FFFF    <Joystick Data><FF><repeated>
    0x320000    0x33FFFF    <Audio Status><FF><repeated>
    0x340000    0x35FFFF    <Joystick Data><FF><repeated>
    0x360000    0x37FFFF    <FF><repeated>
    0x380000    0x39FFFF    <Joystick Data><FF><repeated>
    0x3A0000    0x3BFFFF    <FF><repeated>
    0x3C0000    0x3DFFFF    <03><FF><03><FF><00><FF><counter><FF><repeated>
    0x3E0000    0x3FFFFF    <FF><repeated>
    0x400000    0x4FFFFF    Palette data, mirrored on the whole area. Read access produces snow.
    0x500000    0x7FFFFF    BUS ERROR !
    0x800000    0x8FFFFF    Backup RAM, mirrored on the whole area.
    0x900000    0xBFFFFF    BUS ERROR !
    0xC00000    0xCFFFFF    ROM, mirrored on the whole area.
    0xD00000    0xDFFFFF    BUS ERROR !
    0xE00000    0xEFFFFF    Mapped area.
    0xF00000    0xFFFFFF    BUS ERROR, except for the registers in the FF0000 range.
*/

Memory::Memory() :
    vectorsMappedToRom(false),
    regionLookupTable(nullptr),
    ram(nullptr),
    rom(nullptr),
    sprRam(nullptr),
    fixRam(nullptr),
    pcmRam(nullptr),
    videoRam(nullptr),
    paletteRam(nullptr),
    yZoomRom(nullptr),
    z80Ram(nullptr),
    backupRam(nullptr),
    sprBankSelect(0),
    pcmBankSelect(0),
    busRequest(0),
    areaSelect(0),
    dmaSource(0),
    dmaDestination(0),
    dmaLength(0),
    dmaPattern(0),
    memoryRegions(),
    vectorRegions()
{
    // Allocate the region lookup table
    regionLookupTable = reinterpret_cast<const Memory::Region**>(std::calloc(0x1000000 / MEMORY_GRANULARITY, sizeof(Memory::Region*)));

    // Program RAM: 2MiB
    ram = reinterpret_cast<uint8_t*>(std::malloc(RAM_SIZE));

    // ROM: 512KiB
    rom = reinterpret_cast<uint8_t*>(std::malloc(ROM_SIZE));

    // SPR: 4MiB
    sprRam = reinterpret_cast<uint8_t*>(std::malloc(SPRRAM_SIZE));

    // FIX: 128KiB
    fixRam = reinterpret_cast<uint8_t*>(std::malloc(FIXRAM_SIZE));

    // PCM: 1MiB
    pcmRam = reinterpret_cast<uint8_t*>(std::malloc(PCMRAM_SIZE));

    // Video RAM: 128KiB
    videoRam = reinterpret_cast<uint16_t*>(std::malloc(VIDEORAM_SIZE));

    // Palette RAM: 16KiB
    paletteRam = reinterpret_cast<uint16_t*>(std::malloc(PALETTERAM_SIZE));

    // Y Zoom Table ROM: 64KiB
    yZoomRom = reinterpret_cast<uint8_t*>(std::malloc(YZOOMROM_SIZE));

    // Z80 RAM: 64KiB
    z80Ram = reinterpret_cast<uint8_t*>(std::malloc(Z80RAM_SIZE));

    // Backup RAM: 8KiB
    backupRam = reinterpret_cast<uint8_t*>(std::malloc(BACKUPRAM_SIZE));

    buildMemoryMap();
    initializeRegionLookupTable();
    mapVectorsToRom();
}

Memory::~Memory()
{
    if (backupRam)
        std::free(backupRam);

    if (z80Ram)
        std::free(z80Ram);

    if (yZoomRom)
        std::free(yZoomRom);

    if (paletteRam)
        std::free(paletteRam);

    if (videoRam)
        std::free(videoRam);

    if (pcmRam)
        std::free(pcmRam);

    if (fixRam)
        std::free(fixRam);

    if (sprRam)
        std::free(sprRam);

    if (rom)
        std::free(rom);

    if (ram)
        std::free(ram);

    if (regionLookupTable)
        std::free(regionLookupTable);
}

void Memory::reset()
{
    // Wipe clean all RAM
    std::memset(ram, 0, RAM_SIZE);
    std::memset(sprRam, 0, SPRRAM_SIZE);
    std::memset(fixRam, 0, FIXRAM_SIZE);
    std::memset(pcmRam, 0, PCMRAM_SIZE);
    std::memset(videoRam, 0, VIDEORAM_SIZE);
    std::memset(paletteRam, 0, PALETTERAM_SIZE);
    std::memset(z80Ram, 0, Z80RAM_SIZE);

    // Map vector table to ROM
    mapVectorsToRom();

    // Initialize DMA registers
    resetDma();

    busRequest = 0;
    areaSelect = 0;

    sprBankSelect = 0;
    pcmBankSelect = 0;
}

void Memory::mapVectorsToRam()
{
    regionLookupTable[0] = &vectorRegions[1];
    vectorsMappedToRom = false;
}

void Memory::mapVectorsToRom()
{
    regionLookupTable[0] = &vectorRegions[0];
    vectorsMappedToRom = true;
}

void Memory::buildMemoryMap()
{
    memoryRegions.clear();
    vectorRegions.clear();

    memoryRegions.push_back({ static_cast<Memory::Region::Flags>(Memory::Region::ReadDirect | Memory::Region::WriteDirect), 0x000000, 0x1FFFFF, 0x001FFFFF, nullptr, neocd.memory.ram, neocd.memory.ram });
    memoryRegions.push_back({ static_cast<Memory::Region::Flags>(Memory::Region::ReadMapped | Memory::Region::WriteMapped), 0x300000, 0x31FFFF, 0x00000001, &controller1Handlers, nullptr, nullptr });
    memoryRegions.push_back({ static_cast<Memory::Region::Flags>(Memory::Region::ReadMapped | Memory::Region::WriteMapped), 0x320000, 0x33FFFF, 0x00000001, &z80CommunicationHandlers, nullptr, nullptr });
    memoryRegions.push_back({ static_cast<Memory::Region::Flags>(Memory::Region::ReadMapped | Memory::Region::WriteMapped), 0x340000, 0x35FFFF, 0x00000001, &controller2Handlers, nullptr, nullptr });
    memoryRegions.push_back({ static_cast<Memory::Region::Flags>(Memory::Region::ReadMapped | Memory::Region::WriteMapped), 0x380000, 0x39FFFF, 0x00000001, &controller3Handlers, nullptr, nullptr });
    memoryRegions.push_back({ static_cast<Memory::Region::Flags>(Memory::Region::ReadMapped | Memory::Region::WriteMapped), 0x3A0000, 0x3BFFFF, 0x0000001F, &switchHandlers, nullptr, nullptr });
    memoryRegions.push_back({ static_cast<Memory::Region::Flags>(Memory::Region::ReadMapped | Memory::Region::WriteMapped), 0x3C0000, 0x3DFFFF, 0x0000000F, &videoRamHandlers, nullptr, nullptr });
    memoryRegions.push_back({ static_cast<Memory::Region::Flags>(Memory::Region::ReadMapped | Memory::Region::WriteMapped), 0x400000, 0x4FFFFF, 0x00001FFF, &paletteRamHandlers, nullptr, nullptr });
    memoryRegions.push_back({ static_cast<Memory::Region::Flags>(Memory::Region::ReadMapped | Memory::Region::WriteMapped), 0x800000, 0x8FFFFF, 0x00003FFF, &backupRamHandlers, nullptr, nullptr });
    memoryRegions.push_back({ static_cast<Memory::Region::Flags>(Memory::Region::ReadDirect | Memory::Region::WriteNop),    0xC00000, 0xCFFFFF, 0x0007FFFF, nullptr, neocd.memory.rom, nullptr });
    memoryRegions.push_back({ static_cast<Memory::Region::Flags>(Memory::Region::ReadMapped | Memory::Region::WriteMapped), 0xE00000, 0xEFFFFF, 0x000FFFFF, &mappedRamHandlers, nullptr, nullptr });
    memoryRegions.push_back({ static_cast<Memory::Region::Flags>(Memory::Region::ReadMapped | Memory::Region::WriteMapped), 0xFF0000, 0xFF01FF, 0x000001FF, &cdInterfaceHandlers, nullptr, nullptr });

    // Non essential areas

    // The 0x2000000 area is normally random data (whatever is on the data bus?)
    memoryRegions.push_back({ static_cast<Memory::Region::Flags>(Memory::Region::ReadNop | Memory::Region::WriteNop), 0x200000, 0x2FFFFF, 0x00000000, nullptr, nullptr, nullptr });
    memoryRegions.push_back({ static_cast<Memory::Region::Flags>(Memory::Region::ReadNop | Memory::Region::WriteNop), 0x360000, 0x37FFFF, 0x00000000, nullptr, nullptr, nullptr });
    memoryRegions.push_back({ static_cast<Memory::Region::Flags>(Memory::Region::ReadNop | Memory::Region::WriteNop), 0x3E0000, 0x3FFFFF, 0x00000000, nullptr, nullptr, nullptr });

    // Those regions are only used to swap the first 0x80 bytes of the address map between ROM and RAM
    vectorRegions.push_back({ static_cast<Memory::Region::Flags>(Memory::Region::ReadDirect | Memory::Region::WriteNop), 0x000000, 0x00007F, 0x0000007F, nullptr, neocd.memory.rom, nullptr });
    vectorRegions.push_back({ static_cast<Memory::Region::Flags>(Memory::Region::ReadDirect | Memory::Region::WriteDirect), 0x000000, 0x00007F, 0x0000007F, nullptr, neocd.memory.ram, neocd.memory.ram });
}

void Memory::initializeRegionLookupTable()
{
    for (int i = 0; i < static_cast<int>(memoryRegions.size()); ++i)
    {
        for (const Memory::Region** ptr = &regionLookupTable[memoryRegions[i].startAddress / MEMORY_GRANULARITY];
            ptr <= &regionLookupTable[memoryRegions[i].endAddress / MEMORY_GRANULARITY];
            ++ptr)
            *ptr = &memoryRegions[i];
    }
}

void Memory::doDma()
{
    switch (dmaConfig[0])
    {
    case 0xFE3D:
    case 0xFE6D:
        dmaOpCopy();
        break;
    case 0xFFC5:
    case 0xFF89: // Used in Front Loader BIOS
        dmaOpCopyCdrom();
        break;
    case 0xFEF5:
        dmaOpFill();
        break;
    case 0xFFCD:
    case 0xFFDD:
        dmaOpPattern();
        break;
    case 0xE2DD:
    case 0xF2DD: // Used in Front Loader BIOS
        dmaOpCopyOddBytes();
        break;
    case 0xFC2D:
        dmaOpCopyCdromOddBytes();
        break;
    case 0xCFFD:
        dmaOpFillOddBytes();
        break;
    default:
        LOG(LOG_ERROR, "DMA transfer with unknown DMA configuration: %04x %04x %04x %04x %04x %04x %04x %04x %04x\n",
            dmaConfig[0],
            dmaConfig[1],
            dmaConfig[2],
            dmaConfig[3],
            dmaConfig[4],
            dmaConfig[5],
            dmaConfig[6],
            dmaConfig[7],
            dmaConfig[8]);
        LOG(LOG_ERROR, "Source : %X\n", dmaSource);
        LOG(LOG_ERROR, "Dest   : %X\n", dmaDestination);
        LOG(LOG_ERROR, "Length : %X\n", dmaLength);
        LOG(LOG_ERROR, "Pattern: %X\n", dmaPattern);
        LOG(LOG_ERROR, "(PC = %X)\n", m68k_get_reg(NULL, M68K_REG_PPC));
        break;
    }
}

void Memory::resetDma()
{
    std::memset(dmaConfig, 0, sizeof(dmaConfig));
    dmaSource = 0;
    dmaDestination = 0;
    dmaLength = 0;
    dmaPattern = 0;
}

const Memory::Region* Memory::dmaFindRegion(uint32_t address)
{
    address &= 0xFFFFFF;

    auto i = std::find_if(memoryRegions.cbegin(), memoryRegions.cend(), [&](const Memory::Region& region) {
        return ((address >= region.startAddress) && (address <= region.endAddress));
    });

    if (i == memoryRegions.cend())
        return nullptr;

    return &(*i);
}

uint16_t Memory::dmaFetchNextWord(const Memory::Region* region, uint32_t& offset)
{
    uint16_t value;

    if (region->flags & Memory::Region::ReadDirect)
        value = BIG_ENDIAN_WORD(*reinterpret_cast<const uint16_t*>(&region->readBase[offset & region->addressMask]));
    else if (region->flags & Memory::Region::ReadMapped)
        value = region->handlers->readWord(offset & region->addressMask);
    else // Memory::Region::ReadNop
        value = 0xFFFF;

    offset += 2;
    return value;
}

void Memory::dmaWriteNextWord(const Memory::Region* region, uint32_t& offset, uint16_t data)
{
    if (region->flags & Memory::Region::WriteDirect)
        *reinterpret_cast<uint16_t*>(&region->writeBase[offset & region->addressMask]) = BIG_ENDIAN_WORD(data);
    else if (region->flags & Memory::Region::WriteMapped)
        region->handlers->writeWord(offset & region->addressMask, data);

    offset += 2;
}

void Memory::dmaOpCopyCdrom(void)
{
    // Find the destination region
    const Memory::Region* region = dmaFindRegion(dmaDestination);
    if (!region)
    {
        LOG(LOG_ERROR, "DMA COPY FROM CD BUFFER: Unknown destination region.\n");
        LOG(LOG_ERROR, "Dest   : %X\n", dmaDestination);
        LOG(LOG_ERROR, "Length : %X\n", dmaLength);
        LOG(LOG_ERROR, "(PC = %X)\n", m68k_get_reg(NULL, M68K_REG_PPC));
        return;
    }

    /*
        Art of Fighting, after the first two matches will send DMA ops with length = 0x20000 when loading the bonus area selection screen
        This only happens with the CDZ Bios. Not sure what is really happening here. For now we fix it by patching the length :s
    */
    if (dmaLength > 0x400)
    {
        LOG(LOG_ERROR, "DMA transfer from CD buffer with length > 0x400 ! \n");
        m68k_write_memory_32(0x10FEFC, 0x800);
        dmaLength = 0x400;
    }
    else if (dmaLength < 0x400)
        LOG(LOG_ERROR, "DMA transfer from CD buffer with length = %X ! \n", dmaLength);

    if (neocd.lc8951.IFSTAT & LC8951::DTBSY)
        LOG(LOG_ERROR, "DMA transfer from CD buffer but LC8951 side is not started ! \n");

    if (neocd.lc8951.wordRegister(neocd.lc8951.DBCL, neocd.lc8951.DBCH) != 0x7FF)
        LOG(LOG_ERROR, "DMA transfer from CD buffer but LC8951 length is not 0x7FF ! \n");

    uint16_t* source = reinterpret_cast<uint16_t*>(&neocd.lc8951.buffer[0]);
    uint32_t length = dmaLength;
    uint32_t offset = dmaDestination & region->addressMask;
    
    while (length)
    {
        dmaWriteNextWord(region, offset, BIG_ENDIAN_WORD(*source));
        source++;
        length--;
    }
    
    neocd.lc8951.endTransfer();
}

void Memory::dmaOpCopyCdromOddBytes(void)
{
    // Find the destination region
    const Memory::Region* region = dmaFindRegion(dmaDestination);
    if (!region)
    {
        LOG(LOG_ERROR, "DMA COPY FROM CD BUFFER (ODD BYTES): Unknown destination region.\n");
        LOG(LOG_ERROR, "Dest   : %X\n", dmaDestination);
        LOG(LOG_ERROR, "Length : %X\n", dmaLength);
        LOG(LOG_ERROR, "(PC = %X)\n", m68k_get_reg(NULL, M68K_REG_PPC));
        return;
    }

    if (dmaLength > 0x400)
    {
        LOG(LOG_ERROR, "DMA transfer from CD buffer with length > 0x400 ! \n");
        m68k_write_memory_32(0x10FEFC, 0x800); // Fix, see above.
        dmaLength = 0x400;
    }
    else if (dmaLength < 0x400)
        LOG(LOG_ERROR, "DMA transfer from CD buffer with length = %X ! \n", dmaLength);
        
    if (neocd.lc8951.IFSTAT & LC8951::DTBSY)
        LOG(LOG_ERROR, "DMA transfer from CD buffer but LC8951 side is not started ! \n");

    if (neocd.lc8951.wordRegister(neocd.lc8951.DBCL, neocd.lc8951.DBCH) != 0x7FF)
        LOG(LOG_ERROR, "DMA transfer from CD buffer but LC8951 length is not 0x7FF ! \n");

    uint16_t* source = reinterpret_cast<uint16_t*>(&neocd.lc8951.buffer[0]);
    uint16_t data;
    uint32_t length = dmaLength;
    uint32_t offset = dmaDestination & region->addressMask;

    while (length)
    {
        data = BIG_ENDIAN_WORD(*source);
        source++;
        dmaWriteNextWord(region, offset, data >> 8);
        dmaWriteNextWord(region, offset, data);
        length--;
    }
    
    neocd.lc8951.endTransfer();
}

void Memory::dmaOpCopyOddBytes(void)
{
    // NOTE: dmaSource and dmaDestination are reversed for this

    // Find the source region
    const Memory::Region* sourceRegion = dmaFindRegion(dmaDestination);

    // Find the destination region
    const Memory::Region* destinationRegion = dmaFindRegion(dmaSource);

    if ((!sourceRegion) || (!destinationRegion))
    {
        LOG(LOG_ERROR, "DMA COPY ODD BYTES: Unhandled call\n");
        LOG(LOG_ERROR, "Source : %X\n", dmaDestination);
        LOG(LOG_ERROR, "Dest   : %X\n", dmaSource);
        LOG(LOG_ERROR, "Length : %X\n", dmaLength);
        LOG(LOG_ERROR, "Pattern: %X\n", dmaPattern);
        LOG(LOG_ERROR, "(PC = %X)\n", m68k_get_reg(NULL, M68K_REG_PPC));
        return;
    }

    // Do the copy

    uint32_t sourceOffset = dmaDestination & sourceRegion->addressMask;
    uint32_t destinationOffset = dmaSource & destinationRegion->addressMask;
    uint16_t data;
    uint32_t length = dmaLength;

    while (length)
    {
        data = dmaFetchNextWord(sourceRegion, sourceOffset);
        dmaWriteNextWord(destinationRegion, destinationOffset, BYTE_SWAP_16(data));
        dmaWriteNextWord(destinationRegion, destinationOffset, data);
        length--;
    }
}

void Memory::dmaOpCopy(void)
{
    // NOTE: dmaSource and dmaDestination are reversed for this

    // Find the source region
    const Memory::Region* sourceRegion = dmaFindRegion(dmaDestination);

    // Find the destination region
    const Memory::Region* destinationRegion = dmaFindRegion(dmaSource);

    if ((!sourceRegion) || (!destinationRegion))
    {
        LOG(LOG_ERROR, "DMA COPY: Unhandled call\n");
        LOG(LOG_ERROR, "Source : %X\n", dmaDestination);
        LOG(LOG_ERROR, "Dest   : %X\n", dmaSource);
        LOG(LOG_ERROR, "Length : %X\n", dmaLength);
        LOG(LOG_ERROR, "Pattern: %X\n", dmaPattern);
        LOG(LOG_ERROR, "(PC = %X)\n", m68k_get_reg(NULL, M68K_REG_PPC));
        return;
    }

    // Do the copy

    uint32_t sourceOffset = dmaDestination & sourceRegion->addressMask;
    uint32_t destinationOffset = dmaSource & destinationRegion->addressMask;
    uint16_t data;
    uint32_t length = dmaLength;

    while (length)
    {
        data = dmaFetchNextWord(sourceRegion, sourceOffset);
        dmaWriteNextWord(destinationRegion, destinationOffset, data);
        length--;
    }
}

void Memory::dmaOpPattern(void)
{
    // Find the destination region
    const Memory::Region* region = dmaFindRegion(dmaDestination);
    if (!region)
    {
        LOG(LOG_ERROR, "DMA PATTERN: Unknown destination region.\n");
        LOG(LOG_ERROR, "Dest   : %X\n", dmaDestination);
        LOG(LOG_ERROR, "Length : %X\n", dmaLength);
        LOG(LOG_ERROR, "Pattern: %X\n", dmaPattern);
        LOG(LOG_ERROR, "(PC = %X)\n", m68k_get_reg(NULL, M68K_REG_PPC));
        return;
    }

    // Fill by writing the pattern
    uint32_t offset = dmaDestination & region->addressMask;
    uint32_t length = dmaLength;

    while (length)
    {
        dmaWriteNextWord(region, offset, dmaPattern);
        length--;
    }
}

void Memory::dmaOpFill(void)
{
    // Find the destination region
    const Memory::Region* region = dmaFindRegion(dmaDestination);
    if (!region)
    {
        LOG(LOG_ERROR, "DMA FILL: Unknown destination region.\n");
        LOG(LOG_ERROR, "Dest   : %X\n", dmaDestination);
        LOG(LOG_ERROR, "Length : %X\n", dmaLength);
        LOG(LOG_ERROR, "Pattern: %X\n", dmaPattern);
        LOG(LOG_ERROR, "(PC = %X)\n", m68k_get_reg(NULL, M68K_REG_PPC));
        return;
    }

    // Fill by writing the current address

    uint32_t address = dmaDestination;
    uint32_t offset = dmaDestination & region->addressMask;
    uint32_t length = dmaLength;

    while (length)
    {
        dmaWriteNextWord(region, offset, (address >> 16));
        dmaWriteNextWord(region, offset, address);
        address += 4;
        length--;
    }
}

void Memory::dmaOpFillOddBytes(void)
{
    // Find the destination region
    const Memory::Region* region = dmaFindRegion(dmaDestination);
    if (!region)
    {
        LOG(LOG_ERROR, "DMA FILL ODD BYTES: Unknown destination region.\n");
        LOG(LOG_ERROR, "Dest   : %X\n", dmaDestination);
        LOG(LOG_ERROR, "Length : %X\n", dmaLength);
        LOG(LOG_ERROR, "Pattern: %X\n", dmaPattern);
        LOG(LOG_ERROR, "(PC = %X)\n", m68k_get_reg(NULL, M68K_REG_PPC));
        return;
    }

    // Fill by writing the current address

    uint32_t address = dmaDestination;
    uint32_t offset = dmaDestination & region->addressMask;
    uint32_t length = dmaLength;

    while (length)
    {
        dmaWriteNextWord(region, offset, (address >> 24));
        dmaWriteNextWord(region, offset, (address >> 16));
        dmaWriteNextWord(region, offset, (address >> 8));
        dmaWriteNextWord(region, offset, address);
        address += 8;
        length--;
    }
}

void Memory::dumpDebugState()
{
    std::ofstream file;

    file.open("d:/temp/registers.bin", std::ios::out | std::ios::binary);

    auto dumpRegValue = [&](m68k_register_t reg)
    {
        uint32_t regValue = m68k_get_reg(NULL, reg);
        file.write(reinterpret_cast<const char *>(&regValue), sizeof(uint32_t));
    };

    dumpRegValue(M68K_REG_D0);
    dumpRegValue(M68K_REG_D1);
    dumpRegValue(M68K_REG_D2);
    dumpRegValue(M68K_REG_D3);
    dumpRegValue(M68K_REG_D4);
    dumpRegValue(M68K_REG_D5);
    dumpRegValue(M68K_REG_D6);
    dumpRegValue(M68K_REG_D7);
    dumpRegValue(M68K_REG_A0);
    dumpRegValue(M68K_REG_A1);
    dumpRegValue(M68K_REG_A2);
    dumpRegValue(M68K_REG_A3);
    dumpRegValue(M68K_REG_A4);
    dumpRegValue(M68K_REG_A5);
    dumpRegValue(M68K_REG_A6);
    dumpRegValue(M68K_REG_SP);
    dumpRegValue(M68K_REG_PC);
    file.close();

    file.open("d:/temp/ram.bin", std::ios::out | std::ios::binary);
    file.write(reinterpret_cast<const char *>(ram), Memory::RAM_SIZE);
    file.close();
}


DataPacker& operator<<(DataPacker& out, const Memory& memory)
{
    out << memory.vectorsMappedToRom;
    out << memory.dmaConfig;
    out << memory.dmaSource;
    out << memory.dmaDestination;
    out << memory.dmaLength;
    out << memory.dmaPattern;
    out << memory.sprBankSelect;
    out << memory.pcmBankSelect;
    out << memory.busRequest;
    out << memory.areaSelect;

    out.push(reinterpret_cast<const char*>(memory.ram), Memory::RAM_SIZE);
    out.push(reinterpret_cast<const char*>(memory.rom), Memory::ROM_SIZE);
    out.push(reinterpret_cast<const char*>(memory.sprRam), Memory::SPRRAM_SIZE);
    out.push(reinterpret_cast<const char*>(memory.fixRam), Memory::FIXRAM_SIZE);
    out.push(reinterpret_cast<const char*>(memory.pcmRam), Memory::PCMRAM_SIZE);
    out.push(reinterpret_cast<const char*>(memory.videoRam), Memory::VIDEORAM_SIZE);
    out.push(reinterpret_cast<const char*>(memory.paletteRam), Memory::PALETTERAM_SIZE);
    out.push(reinterpret_cast<const char*>(memory.z80Ram), Memory::Z80RAM_SIZE);

    return out;
}

DataPacker& operator>>(DataPacker& in, Memory& memory)
{
    in >> memory.vectorsMappedToRom;

    if (memory.vectorsMappedToRom)
        memory.mapVectorsToRom();
    else
        memory.mapVectorsToRam();

    in >> memory.dmaConfig;
    in >> memory.dmaSource;
    in >> memory.dmaDestination;
    in >> memory.dmaLength;
    in >> memory.dmaPattern;
    in >> memory.sprBankSelect;
    in >> memory.pcmBankSelect;
    in >> memory.busRequest;
    in >> memory.areaSelect;

    in.pop(reinterpret_cast<char*>(memory.ram), Memory::RAM_SIZE);
    in.pop(reinterpret_cast<char*>(memory.rom), Memory::ROM_SIZE);
    in.pop(reinterpret_cast<char*>(memory.sprRam), Memory::SPRRAM_SIZE);
    in.pop(reinterpret_cast<char*>(memory.fixRam), Memory::FIXRAM_SIZE);
    in.pop(reinterpret_cast<char*>(memory.pcmRam), Memory::PCMRAM_SIZE);
    in.pop(reinterpret_cast<char*>(memory.videoRam), Memory::VIDEORAM_SIZE);
    in.pop(reinterpret_cast<char*>(memory.paletteRam), Memory::PALETTERAM_SIZE);
    in.pop(reinterpret_cast<char*>(memory.z80Ram), Memory::Z80RAM_SIZE);

    neocd.video.convertPalette();
    neocd.video.updateFixUsageMap();

    return in;
}

#ifndef MEMORY_H
#define MEMORY_H

#include <cstddef>
#include <cstdint>
#include <array>

#include "neocd_endian.h"
#include "datapacker.h"

class Memory
{
public:
    static constexpr uint32_t MEMORY_GRANULARITY = 0x80;
    static constexpr size_t RAM_SIZE = 0x200000;
    static constexpr size_t ROM_SIZE = 0x80000;
    static constexpr size_t SPRRAM_SIZE = 0x400000;
    static constexpr size_t FIXRAM_SIZE = 0x20000;
    static constexpr size_t PCMRAM_SIZE = 0x100000;
    static constexpr size_t VIDEORAM_SIZE = 0x20000;
    static constexpr size_t PALETTERAM_SIZE = 0x4000;
    static constexpr size_t YZOOMROM_SIZE = 0x10000;
    static constexpr size_t Z80RAM_SIZE = 0x10000;
    static constexpr size_t BACKUPRAM_SIZE = 0x2000;

    typedef uint32_t(*ReadHandler)(uint32_t address);
    typedef void(*WriteHandler)(uint32_t address, uint32_t data);

    struct Handlers
    {
        Memory::ReadHandler readByte;
        Memory::ReadHandler readWord;
        Memory::WriteHandler writeByte;
        Memory::WriteHandler writeWord;
    };

    class Regions
    {
    public:
        enum
        {
            RAM,
            Unused20,
            Controller1,
            Z80Comm,
            Controller2,
            Unused36,
            Controller3,
            Switches,
            Video,
            Unused3E,
            Palette,
            Backup,
            ROM,
            MappedRAM,
            CDInterface,
            Count
        };
    };

    class Vectors
    {
    public:
        enum
        {
            ROM,
            RAM,
            Count
        };
    };

    struct Region
    {
        enum Flags
        {
            ReadNop = 0x01,
            ReadMapped = 0x02,
            ReadDirect = 0x04,
            WriteNop = 0x08,
            WriteMapped = 0x10,
            WriteDirect = 0x20
        };

        uint32_t flags;
        uint32_t startAddress;
        uint32_t endAddress;
        uint32_t addressMask;
        const Memory::Handlers* handlers;
        const uint8_t* readBase;
        uint8_t* writeBase;
    };

    enum MemoryArea
    {
        AREA_SPR = 1,
        AREA_PCM = 2,
        AREA_Z80 = 4,
        AREA_FIX = 8
    };

    Memory();
    ~Memory();

    // Non copyable
    Memory(const Memory&) = delete;
    
    // Non copyable
    Memory& operator=(const Memory&) = delete;

    void reset();
    void mapVectorsToRam();
    void mapVectorsToRom();
    void doDma();
    void resetDma();

    uint8_t* yZoomRom;
    uint8_t* backupRam;

    // Variables to save in savestate
    bool        vectorsMappedToRom;
    uint16_t    dmaConfig[9];
    uint32_t    dmaSource;
    uint32_t    dmaDestination;
    uint32_t    dmaLength;
    uint16_t    dmaPattern;
    uint32_t    sprBankSelect;
    uint32_t    pcmBankSelect;
    uint32_t    busRequest;
    uint32_t    areaSelect;
    // Memory areas to save in savestate
    uint8_t*    ram;
    uint8_t*    rom;
    uint8_t*    sprRam;
    uint8_t*    fixRam;
    uint8_t*    pcmRam;
    uint16_t*   videoRam;
    uint16_t*   paletteRam;
    uint8_t*    z80Ram;
    // End variables to save in savestate

    const Memory::Region** regionLookupTable;

    friend DataPacker& operator<<(DataPacker& out, const Memory& memory);
    friend DataPacker& operator>>(DataPacker& in, Memory& memory);

protected:
    void buildMemoryMap();
    void initializeRegionLookupTable();
    void generateYZoomData();

    const Memory::Region* dmaFindRegion(uint32_t address);
    uint16_t dmaFetchNextWord(const Memory::Region* region, uint32_t& offset);
    void dmaWriteNextWord(const Memory::Region* region, uint32_t& offset, uint16_t data);
    void dmaOpCopyCdrom(void);
    void dmaOpCopyCdromOddBytes(void);
    void dmaOpCopyOddBytes(void);
    void dmaOpCopy(void);
    void dmaOpPattern(void);
    void dmaOpFill(void);
    void dmaOpFillOddBytes(void);

    void dumpDebugState();
    
    std::array<Memory::Region, Regions::Count> memoryRegions;
    std::array<Memory::Region, Vectors::Count> vectorRegions;
};

DataPacker& operator<<(DataPacker& out, const Memory& memory);
DataPacker& operator>>(DataPacker& in, Memory& memory);

#endif // MEMORY_H

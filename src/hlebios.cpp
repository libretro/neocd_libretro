#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

#include "3rdparty/musashi/m68k.h"
#include "3rdparty/ym/ym2610.h"
#include "3rdparty/z80/z80.h"
#include "hlebios.h"
#include "libretro_common.h"
#include "libretro_log.h"
#include "neogeocd.h"
#include "z80intf.h"

bool HleBios::m_booted = false;
uint32_t HleBios::m_rootLba = 0;
uint32_t HleBios::m_rootSize = 0;
uint8_t HleBios::m_userRequest = 0;
uint32_t HleBios::m_userDelay = 0;
uint8_t HleBios::m_startLatch = 0;
uint32_t HleBios::m_busyFrames = 0;
uint32_t HleBios::m_heartbeatFrames = 0;
uint32_t HleBios::m_playUntil = 0;
bool HleBios::m_loopAtEnd = false;
uint8_t HleBios::m_lastP1 = 0;
uint8_t HleBios::m_lastP2 = 0;
uint8_t HleBios::m_lastStatus = 0;

// 68000 opcodes the synthesised ROM is built from.
static constexpr uint16_t OP_ILLEGAL = 0x4AFC;
static constexpr uint16_t OP_RTS     = 0x4E75;
static constexpr uint16_t OP_RTE     = 0x4E73;

static inline void poke16(uint8_t* rom, uint32_t address, uint16_t value)
{
    uint32_t offset = address - HleBios::ROM_BASE;
    rom[offset]     = static_cast<uint8_t>(value >> 8);
    rom[offset + 1] = static_cast<uint8_t>(value & 0xFF);
}

static inline void poke32(uint8_t* rom, uint32_t address, uint32_t value)
{
    poke16(rom, address, static_cast<uint16_t>(value >> 16));
    poke16(rom, address + 2, static_cast<uint16_t>(value & 0xFFFF));
}

// An entry point is an illegal instruction followed by a return: the
// illegal traps into C++, and the return runs afterwards as if the
// routine had been written in 68000 code all along.
static inline void entryPoint(uint8_t* rom, uint32_t address, uint16_t returnOp)
{
    poke16(rom, address, OP_ILLEGAL);
    poke16(rom, address + 2, returnOp);
}

void HleBios::reset()
{
    m_booted = false;
    m_rootLba = 0;
    m_rootSize = 0;
}

void HleBios::buildRom(uint8_t* rom)
{
    // Anything not deliberately placed below is an illegal instruction,
    // so a game that calls a routine this does not implement stops here
    // rather than running through whatever bytes happened to be around.
    for (uint32_t offset = 0; offset < ROM_SIZE; offset += 2)
    {
        rom[offset]     = static_cast<uint8_t>(OP_ILLEGAL >> 8);
        rom[offset + 1] = static_cast<uint8_t>(OP_ILLEGAL & 0xFF);
    }

    // Reset vector. The stack goes where the real BIOS puts it, since
    // games assume the layout of what is below it.
    poke32(rom, ROM_BASE + 0x00, 0x0010F300);
    poke32(rom, ROM_BASE + 0x04, BOOT);

    // Exception vectors. Level 1 is the vertical blank; the rest are
    // pointed at a handler that does nothing but return.
    for (uint32_t vector = 2; vector < 64; ++vector)
        poke32(rom, ROM_BASE + vector * 4, IGNORE_IRQ);

    poke32(rom, ROM_BASE + 0x64, VBLANK);       // level 1 autovector
    poke32(rom, ROM_BASE + 0x68, IGNORE_IRQ);   // level 2 autovector
    poke32(rom, ROM_BASE + 0x6C, IGNORE_IRQ);   // level 3 autovector

    entryPoint(rom, BOOT, OP_RTS);
    entryPoint(rom, VBLANK, OP_RTE);
    entryPoint(rom, IGNORE_IRQ, OP_RTE);
    entryPoint(rom, USER_RETURN, OP_RTS);

    // Somewhere to stop. A fault returns here rather than to the
    // instruction that faulted, which would fault again forever and
    // walk the stack out of memory.
    poke16(rom, HALT, 0x60FE);   // BRA.S to itself

    // Where a BIOS waits between a game handing the machine back and
    // being given it again. Interrupts are open here, so the game's own
    // handler still runs - and finds the top bit of the mode byte
    // clear, which is its cue to do nothing but end the interrupt.
    poke16(rom, IDLE, 0x60FE);   // BRA.S to itself

    static const uint32_t calls[] = {
        SYSTEM_RETURN, SYSTEM_IO, FRAME_UPDATE, CREDIT_DOWN,
        READ_CALENDAR, SETUP_CALENDAR, CARD, CARD_ERROR,
        HOW_TO_PLAY, CHECKSUM
    };

    for (uint32_t address : calls)
        entryPoint(rom, address, OP_RTS);

    // A game finishes its vertical blank handler by jumping to
    // SYSTEM_INT1 rather than returning, so these end the exception
    // rather than return from a subroutine.
    entryPoint(rom, SYSTEM_INT1, OP_RTE);
    entryPoint(rom, SYSTEM_INT2, OP_RTE);

    // The exception handlers a game points its vector table at. Reset
    // re-enters the boot; the rest are faults.
    entryPoint(rom, EXC_START, OP_RTS);
    entryPoint(rom, EXC_BUS_ERROR, OP_RTE);
    entryPoint(rom, EXC_ADDR_ERROR, OP_RTE);
    entryPoint(rom, EXC_ILLEGAL, OP_RTE);
    entryPoint(rom, EXC_PRIVILEGE, OP_RTE);
    entryPoint(rom, EXC_TRACE, OP_RTE);
    entryPoint(rom, EXC_SPURIOUS, OP_RTE);
    entryPoint(rom, EXC_UNINIT, OP_RTE);

    // The CD interrupts, and the traps.
    entryPoint(rom, CD_IRQ_54, OP_RTE);
    entryPoint(rom, CD_IRQ_58, OP_RTE);
    entryPoint(rom, CD_IRQ_5C, OP_RTE);
    entryPoint(rom, CD_IRQ_60, OP_RTE);

    for (uint32_t i = 0; i < 5; ++i)
        entryPoint(rom, TRAP_BASE + i * 6, OP_RTE);

    entryPoint(rom, CD_PLAY_TRACK, OP_RTS);
    entryPoint(rom, CD_QUIET_1, OP_RTS);
    entryPoint(rom, CLEAR_TEXT, OP_RTS);
    entryPoint(rom, CD_STREAM_START, OP_RTS);

    /* The sine table at 0xC04000, which games read straight out of the
       BIOS ROM. Fatal Fury 2 shapes the spinning globe of its versus
       screen from it - each column's place and width comes from an
       entry - and with nothing here the columns all fell in a heap.
       The table is the magnitude of the sine, one word per 1/256th of
       a turn, scaled so a quarter turn reads 0x10000 - stored in
       sixteen bits, so the two peaks keep only their carry and read
       0x0001. The quarter is spelled out below rather than computed,
       so that every build of this file produces the same bytes with no
       floating point and no library in the loop; the rest of the table
       is its mirror. Two entries in the second quarter of a real
       BIOS's table have digits transposed - 0xBDAF written 0xBDFA,
       0x6D74 written 0x67D4 - while their twins in the first quarter
       are right, so the original was written out by hand at least that
       far, and the second half repeats the mistakes at the same
       places. They are reproduced: what a game reads from this table
       has to be what a game has always read, slips included.
    */
    static const uint16_t sineQuarter[65] = {
        0x0000, 0x0648, 0x0C90, 0x12D5, 0x1918, 0x1F56, 0x2590, 0x2BC4,
        0x31F1, 0x3817, 0x3E34, 0x4447, 0x4A50, 0x504D, 0x563E, 0x5C22,
        0x61F8, 0x67BE, 0x6D74, 0x731A, 0x78AD, 0x7E2F, 0x839C, 0x88F6,
        0x8E3A, 0x9368, 0x9880, 0x9D80, 0xA268, 0xA736, 0xABEB, 0xB086,
        0xB505, 0xB968, 0xBDAF, 0xC1D8, 0xC5E4, 0xC9D1, 0xCD9F, 0xD14D,
        0xD4DB, 0xD848, 0xDB94, 0xDEBE, 0xE1C6, 0xE4AA, 0xE76C, 0xEA0A,
        0xEC83, 0xEED9, 0xF109, 0xF314, 0xF4FA, 0xF6BA, 0xF854, 0xF9C8,
        0xFB15, 0xFC3B, 0xFD3B, 0xFE13, 0xFEC4, 0xFF4E, 0xFFB1, 0xFFEC,
        0x0001
    };

    for (uint32_t k = 0; k < 256; k++)
    {
        uint32_t fold = k & 127;
        if (fold > 64)
            fold = 128 - fold;

        uint32_t value = sineQuarter[fold];

        if ((k & 127) == 94)
            value = 0xBDFA;
        if ((k & 127) == 110)
            value = 0x67D4;

        rom[0x4000 + k * 2] = static_cast<uint8_t>(value >> 8);
        rom[0x4000 + k * 2 + 1] = static_cast<uint8_t>(value);
    }
    entryPoint(rom, CD_STREAM_ALT, OP_RTS);
    entryPoint(rom, SOUND_COMMAND, OP_RTS);
    entryPoint(rom, CLEAR_SPRITES, OP_RTS);
    entryPoint(rom, CD_QUIET_2, OP_RTS);
    entryPoint(rom, FRAME_UPDATE, OP_RTS);
    entryPoint(rom, UPLOAD, OP_RTS);

    // The random number table. What matters is that the values are well
    // spread and unchanging, not what they are, so these are generated
    // rather than reproduced: a game indexes the table with a counter
    // and wants a different answer each time it looks.
    {
        // What sits at 0xC04200 in a BIOS is not a stream of random
        // bytes: it is the values 0 to 255 each appearing exactly once,
        // dealt out in a scrambled order. Games read it directly - one
        // takes its attract decisions from (table[i] & 7) - and that
        // works because a shuffled deck lands every three bit value
        // exactly thirty-two times. The stream this used to generate had
        // only 160 distinct values and a lopsided spread, so decisions
        // that should come up evenly came up rarely or never: with it,
        // Samurai Shodown 2 triggered 206 sounds over a mashing run
        // where a real table gives 543 and a real BIOS 991.
        //
        // So: the identity table, shuffled. The order is this
        // implementation's own; the property games rely on holds.
        uint32_t state = 0x2545F491;

        for (uint32_t i = 0; i < RANDOM_TABLE_SIZE; ++i)
            rom[(RANDOM_TABLE - ROM_BASE) + i] = static_cast<uint8_t>(i);

        for (uint32_t i = RANDOM_TABLE_SIZE - 1; i > 0; --i)
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;

            uint32_t j = state % (i + 1);
            uint8_t* t = rom + (RANDOM_TABLE - ROM_BASE);
            uint8_t tmp = t[i];
            t[i] = t[j];
            t[j] = tmp;
        }
    }
}

bool HleBios::booted()
{
    return m_booted;
}

bool HleBios::readSector(uint32_t lba, uint8_t* out)
{
    // The table of contents numbers the first track's sectors from
    // zero, which is what the file system's sector numbers count in
    // too, so the two are the same figure.
    neocd->cdrom.seek(lba);

    if (!neocd->cdrom.isData())
    {
        fprintf(stderr, "HLE FAIL: readSector %u lands on non-data\n", lba);
        return false;
    }

    neocd->cdrom.readData(reinterpret_cast<char*>(out));
    return true;
}

static inline uint32_t leWord(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}

bool HleBios::findFile(const std::string& wanted, uint32_t& lba, uint32_t& size)
{
    // A name on the disc carries a version after a semicolon, and what a
    // game asks for may carry one too - IPL.TXT tends not to, a game's
    // own list often does. Neither is part of the name, so both sides
    // lose it before they are compared.
    std::string name = wanted;
    size_t version = name.find(';');
    if (version != std::string::npos)
        name = name.substr(0, version);

    // The root directory size comes straight from the volume descriptor,
    // which a malicious or corrupt disc controls. Reject an absurd value
    // before it becomes a huge allocation, and round the buffer up to a
    // whole number of sectors: readSector always writes a full 2048 bytes,
    // so a size that is not sector-aligned would overflow the vector on
    // the final sector.
    static const uint32_t MAX_ROOT_DIR_SIZE = 1u << 20;

    // Where the directory lives is worked out when the disc is read and
    // kept here, which is not somewhere a savestate reaches. Load a state
    // into a session that has not booted this disc and it would be zero,
    // and every file a game asked for from then on would be reported
    // missing. Read it again rather than fail.
    if (!m_rootSize && !readVolumeDescriptor())
        return false;

    if ((!m_rootSize) || (m_rootSize > MAX_ROOT_DIR_SIZE))
        return false;

    const uint32_t sectorCount = (m_rootSize + 2047) / 2048;
    std::vector<uint8_t> dir(static_cast<size_t>(sectorCount) * 2048, 0);

    for (uint32_t i = 0; i < sectorCount; ++i)
    {
        if (!readSector(m_rootLba + i, &dir[static_cast<size_t>(i) * 2048]))
            return false;
    }

    uint32_t p = 0;
    while (p < m_rootSize)
    {
        uint32_t length = dir[p];

        // A directory record never straddles a sector, so the tail of
        // each one is padded with zeroes; that is a skip to the next
        // sector rather than the end of the directory.
        if (!length)
        {
            p = (p + 2048) & ~static_cast<uint32_t>(2047);
            continue;
        }

        // Every field below is read at a fixed offset from p, and the
        // name runs past those. A record that would reach beyond the
        // directory, or is too short to hold the fixed fields, or whose
        // name spills past its own length, lets the disc steer these
        // reads out of bounds. Stop rather than read past the buffer.
        if ((length < 33) || ((p + length) > m_rootSize))
            break;

        uint32_t nameLength = dir[p + 32];
        if (nameLength > (length - 33))
            break;

        std::string entry(reinterpret_cast<const char*>(&dir[p + 33]), nameLength);

        // Names carry a version suffix that the boot list does not.
        size_t semicolon = entry.find(';');
        if (semicolon != std::string::npos)
            entry = entry.substr(0, semicolon);

        if (entry == name)
        {
            lba = leWord(&dir[p + 2]);
            size = leWord(&dir[p + 10]);
            return true;
        }

        p += length;
    }

    return false;
}

bool HleBios::readFile(uint32_t lba, uint32_t size, std::vector<uint8_t>& out)
{
    uint32_t sectors = (size + 2047) / 2048;
    out.assign(static_cast<size_t>(sectors) * 2048, 0);

    for (uint32_t i = 0; i < sectors; ++i)
    {
        if (!readSector(lba + i, &out[static_cast<size_t>(i) * 2048]))
            return false;
    }

    out.resize(size);
    return true;
}

bool HleBios::parseIpl(const std::vector<uint8_t>& text, std::vector<IplEntry>& entries)
{
    std::string line;

    for (size_t i = 0; i <= text.size(); ++i)
    {
        char c = (i < text.size()) ? static_cast<char>(text[i]) : '\n';

        if ((c != '\n') && (c != '\r'))
        {
            line.push_back(c);
            continue;
        }

        if (line.empty())
            continue;

        // NAME,bank,offset - the offset is hexadecimal, the bank is not.
        size_t first = line.find(',');
        size_t second = line.find(',', first + 1);
        if ((first == std::string::npos) || (second == std::string::npos))
        {
            line.clear();
            continue;
        }

        IplEntry entry;
        entry.name = line.substr(0, first);
        entry.bank = static_cast<uint32_t>(strtoul(line.substr(first + 1, second - first - 1).c_str(), nullptr, 10));
        entry.offset = static_cast<uint32_t>(strtoul(line.substr(second + 1).c_str(), nullptr, 16));
        entries.push_back(entry);

        line.clear();
    }

    return !entries.empty();
}

bool HleBios::loadIplEntry(const IplEntry& entry)
{
    uint32_t lba = 0;
    uint32_t size = 0;

    if (!findFile(entry.name, lba, size))
    {
        fprintf(stderr, "HLE FAIL: file not found: %s\n", entry.name.c_str());
        Libretro::Log::message(RETRO_LOG_ERROR, "HLE BIOS: %s is listed in IPL.TXT but not on the disc.\n", entry.name.c_str());
        return false;
    }

    std::vector<uint8_t> data;
    if (!readFile(lba, size, data))
        return false;

    // Which memory a file goes to is decided by its extension; the real
    // BIOS picks the upload target the same way.
    // The version a disc puts after a semicolon is not part of the name,
    // and it is not part of the extension either - leaving it on turns
    // FIX into FIX;1 and the file goes nowhere.
    std::string bare = entry.name;
    size_t version = bare.find(';');
    if (version != std::string::npos)
        bare = bare.substr(0, version);

    std::string ext;
    size_t dot = bare.rfind('.');
    if (dot != std::string::npos)
        ext = bare.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);

    uint8_t* destination = nullptr;
    size_t capacity = 0;
    uint32_t offset = entry.offset;

    if (ext == "PRG")
    {
        destination = neocd->memory.ram;
        capacity = Memory::RAM_SIZE;
    }
    else if (ext == "FIX")
    {
        destination = neocd->memory.fixRam;
        capacity = Memory::FIXRAM_SIZE;
    }
    else if (ext == "SPR")
    {
        destination = neocd->memory.sprRam;
        capacity = Memory::SPRRAM_SIZE;
        offset += entry.bank * 0x100000;
    }
    else if (ext == "PCM")
    {
        // Sample memory is addressed through a window that counts two
        // bytes of address to one of data, and the bank picks which half
        // megabyte the window sits over - the same arithmetic the mapped
        // path does when a game reaches this memory itself. So the offset
        // asked for is halved and the bank adds half a megabyte:
        //
        //   JOCHU.PCM   bank 0  offset 0        -> 0x00000
        //   SND_00.PCM  bank 1  offset 0        -> 0x80000
        //   SND_0C.PCM  bank 1  offset 0x80000  -> 0xC0000
        //
        // which is where a real BIOS puts them. Taking the offset at
        // face value put samples over the top of each other, quietly.
        destination = neocd->memory.pcmRam;
        capacity = Memory::PCMRAM_SIZE;
        offset = (offset >> 1) + ((entry.bank & 1) * 0x80000);
    }
    else if (ext == "PAT")
    {
        // Not palette data, which is what this loader used to make of
        // the extension. A .PAT file rides alongside a .PCM file and
        // patches the sound driver: the driver's sample tables were
        // written for the cartridge's sample ROM, and every sample now
        // sits wherever the CD version's files were loaded instead. Each
        // ten byte record names a spot in the driver and the sample's
        // place within the sibling file:
        //
        //   [where in the driver][start][end][second start][second end]
        //
        // all big-endian, offsets in file bytes - two of which make one
        // sample byte, the same halving the PCM load does. What lands in
        // the driver is little-endian, in 256 byte units, with the
        // sibling's load address added on and the end made inclusive.
        // The first pair goes at the named address; the second, when it
        // is not zeroes, five bytes further on - the driver keeps a byte
        // of its own between them. Read out of a real run rather than
        // guessed: JOCHU.PAT says put 0090..011C at 4D0C twice over, and
        // a real BIOS leaves 0048 008D at both 4D0C and 4D11.
        //
        // With these skipped, a driver asked for a song looked up its
        // notes at cartridge addresses and found nothing to play: the
        // same command that keys sixty-five notes under a real BIOS
        // keyed none here.
        uint32_t base = (((entry.bank & 1) * 0x80000) + (entry.offset >> 1)) >> 8;

        for (size_t at = 0; at + 10 <= data.size(); at += 10)
        {
            uint32_t where = (static_cast<uint32_t>(data[at]) << 8) | data[at + 1];

            // A record of zeroes separates sections and pads the file
            // out; it is not the end of the list. The first attempt here
            // treated it as data and wrote over the driver's entry
            // point; the second treated it as the end and dropped every
            // record after the first section. It is neither: skip it.
            if ((where == 0x0000) || (where == 0xFFFF))
                continue;

            for (int half = 0; half < 2; ++half)
            {
                size_t f = at + 2 + (half * 4);
                uint32_t start = (static_cast<uint32_t>(data[f]) << 8) | data[f + 1];
                uint32_t end   = (static_cast<uint32_t>(data[f + 2]) << 8) | data[f + 3];
                uint32_t spot = where + (half * 5);

                if (half && !start && !end)
                    break;

                uint32_t startValue = base + (start >> 1);
                uint32_t endValue = base + (end >> 1);

                if (endValue)
                    endValue--;

                if ((spot + 3) < Memory::Z80RAM_SIZE)
                {
                    neocd->memory.z80Ram[spot]     = static_cast<uint8_t>(startValue);
                    neocd->memory.z80Ram[spot + 1] = static_cast<uint8_t>(startValue >> 8);
                    neocd->memory.z80Ram[spot + 2] = static_cast<uint8_t>(endValue);
                    neocd->memory.z80Ram[spot + 3] = static_cast<uint8_t>(endValue >> 8);
                }
            }
        }

        Libretro::Log::message(RETRO_LOG_INFO, "HLE BIOS: applied %-15s %7zu bytes -> Z80 driver\n",
            entry.name.c_str(), data.size());
        return true;
    }
    else if (ext == "Z80")
    {
        destination = neocd->memory.z80Ram;
        capacity = Memory::Z80RAM_SIZE;
    }
    else
    {
        Libretro::Log::message(RETRO_LOG_WARN, "HLE BIOS: no destination for %s, skipped.\n", entry.name.c_str());
        return true;
    }

    // A bank can name more banks than the area has, and the hardware
    // answers that by ignoring the bits it has no room for rather than
    // by refusing - the sprite bank select is two bits wide whatever a
    // game writes to it. So an address past the end wraps to the front,
    // as it would on the machine. King of Fighters '99 asks for sprite
    // data at 0x540000 in a four megabyte area and expects bank five to
    // mean bank one.
    if ((capacity & (capacity - 1)) == 0)
        offset &= static_cast<uint32_t>(capacity - 1);

    if (data.size() > capacity)
    {
        Libretro::Log::message(RETRO_LOG_ERROR,
            "HLE BIOS: %s does not fit at 0x%X (%zu bytes into %zu).\n",
            entry.name.c_str(), offset, data.size(), capacity);
        return false;
    }

    // Data that runs off the end comes round to the front, for the same
    // reason a bank past the end does: the address is masked on every
    // access, so an area behaves as a ring rather than as something with
    // an end to fall off. King of Fighters '99 puts 320K of samples at
    // 0xC0000 in a megabyte and expects the tail at the bottom.
    size_t first = capacity - offset;
    if (first > data.size())
        first = data.size();

    std::memcpy(destination + offset, data.data(), first);
    if (first < data.size())
        std::memcpy(destination, data.data() + first, data.size() - first);

    // The data is here at once, which is worth keeping - nobody wants a
    // loading screen back. What is not free is the time a drive would
    // have spent getting it: at 75 sectors a second, this file would
    // still have been arriving for a while yet. A game that watches the
    // drive rather than the data needs that time to pass, so it is
    // counted here and let run down a frame at a time, without holding
    // the data up.
    // The data is here at once either way. What can be counted is the
    // time a drive would have spent fetching it - some games watch the
    // drive rather than the data, and want to see it busy for a moment
    // afterwards. Skip CD Loading turns that off, which is how this
    // behaved before there was a count at all.
    //
    // Capped either way, and the cap is the point: a drive really would
    // take half a minute over a boot load, but on hardware a game is not
    // running while that happens. Here it is, and already showing its
    // title, so counting the whole of it meant Metal Slug 2 ignoring
    // start for twenty-five seconds after it had finished loading.
    if (!globals.skipCDLoading)
    {
        m_busyFrames += static_cast<uint32_t>(((data.size() + 2047) / 2048) * 60 / 75) + 1;

        if (m_busyFrames > 90)
            m_busyFrames = 90;
    }

    Libretro::Log::message(RETRO_LOG_INFO, "HLE BIOS: loaded %-16s %7zu bytes -> %s+0x%X\n",
        entry.name.c_str(), data.size(), ext.c_str(), offset);

    // Keep the heartbeat below running for about the time this load
    // would really have taken, even when the pacing itself is switched
    // off: a game that checks whether the drive is moving deserves to
    // see it move.
    m_heartbeatFrames = 30;

    /* The fix drawing keeps a map of which characters are empty so it
       can skip them, and every other road into fix memory - the CD
       sector path, DMA, and now the mapped window - tells it about
       writes. This loader wrote around all of them, so a game whose
       text lives in characters it never touches again after loading -
       Fatal Fury 2's menus, cursor and health bars - was showing
       nothing where the map still said empty.
    */
    if (ext == "FIX")
        neocd->video.updateFixUsageMap();

    return true;
}

void HleBios::streamFiles(uint32_t listAddress)
{
    {
        // A BIOS parks the game's frame vector here while it streams and
        // its CD machinery treats the slot being filled as the sign that
        // a game is up and streaming. Games check the same things a BIOS
        // checks, so the slot is filled the same way.
        uint32_t vec = m68k_read_memory_32(0x68);
        uint8_t* ram = neocd->memory.ram;

        ram[0x10F6EE] = static_cast<uint8_t>(vec >> 24);
        ram[0x10F6EF] = static_cast<uint8_t>(vec >> 16);
        ram[0x10F6F0] = static_cast<uint8_t>(vec >> 8);
        ram[0x10F6F1] = static_cast<uint8_t>(vec);
    }

    // What a game asks for when it moves from one screen to the next:
    // the tiles and sprites the next screen needs. A BIOS reads the list,
    // works out which of them are not already resident and fetches those
    // off the disc. Nothing here does the working out - everything asked
    // for is loaded - but the effect a game is after is the same.
    //
    // The list is a run of entries, each a name and where to put it:
    //
    //   "OBJ_04.SPR" 00 | bank | (even) destination
    //
    // which is the same name, bank and offset that IPL.TXT gives, so the
    // same loader handles both.
    uint32_t a = listAddress;

    for (uint32_t guard = 0; guard < 64; ++guard)
    {
        if (!m68k_read_memory_8(a))
            break;

        IplEntry entry;
        entry.name.clear();

        for (uint32_t i = 0; i < 32; ++i)
        {
            uint8_t c = static_cast<uint8_t>(m68k_read_memory_8(a++));
            if (!c)
                break;
            // Names are held uppercase on the disc; a game may not be.
            if ((c >= 'a') && (c <= 'z'))
                c = static_cast<uint8_t>(c - 0x20);
            entry.name.push_back(static_cast<char>(c));
        }

        entry.bank = static_cast<uint32_t>(m68k_read_memory_8(a++));

        // The destination sits on the next even address.
        a = (a + 1) & ~static_cast<uint32_t>(1);
        entry.offset = m68k_read_memory_32(a);
        a += 4;

        if (entry.name.empty())
            break;

        loadIplEntry(entry);
    }
}

bool HleBios::readVolumeDescriptor()
{
    uint8_t sector[2048];

    if (!readSector(16, sector))
        return false;

    if (std::memcmp(&sector[1], "CD001", 5) != 0)
        return false;

    m_rootLba = leWord(&sector[156 + 2]);
    m_rootSize = leWord(&sector[156 + 10]);
    return true;
}

bool HleBios::loadDisc()
{
    uint8_t sector[2048];

    // The volume descriptor is always at the same place.
    if (!readSector(16, sector))
    {
        Libretro::Log::message(RETRO_LOG_ERROR, "HLE BIOS: could not read the disc.\n");
        return false;
    }

    if (std::memcmp(&sector[1], "CD001", 5) != 0)
    {
        Libretro::Log::message(RETRO_LOG_ERROR, "HLE BIOS: this is not an ISO 9660 disc.\n");
        return false;
    }

    m_rootLba = leWord(&sector[156 + 2]);
    m_rootSize = leWord(&sector[156 + 10]);

    uint32_t lba = 0;
    uint32_t size = 0;
    if (!findFile("IPL.TXT", lba, size))
    {
        Libretro::Log::message(RETRO_LOG_ERROR, "HLE BIOS: no IPL.TXT, so nothing to boot.\n");
        return false;
    }

    std::vector<uint8_t> text;
    if (!readFile(lba, size, text))
        return false;

    std::vector<IplEntry> entries;
    if (!parseIpl(text, entries))
    {
        Libretro::Log::message(RETRO_LOG_ERROR, "HLE BIOS: IPL.TXT lists nothing to load.\n");
        return false;
    }

    for (const IplEntry& entry : entries)
    {
        if (!loadIplEntry(entry))
            return false;
    }

    return true;
}

void HleBios::buildTrackTable()
{
    // Where a BIOS writes down the disc: two bytes a track, the minute
    // and the second the track starts at, each as a pair of decimal
    // digits in a byte. Track one comes out at 00:02 because the count
    // starts two seconds before the first sector, as it does on a disc.
    //
    // Nothing filled this in, so games and the CD routines that work out
    // where a track begins were reading zero for every one of them.
    uint8_t* ram = neocd->memory.ram;
    uint8_t highest = 0;

    auto write = [ram](uint8_t slot, uint32_t sector)
    {
        uint32_t frames = sector + 150;
        uint32_t seconds = frames / 75;
        uint32_t minutes = seconds / 60;

        seconds %= 60;

        ram[BIOS_TRACK_TABLE + slot * 2]     =
            static_cast<uint8_t>(((minutes / 10) << 4) | (minutes % 10));
        ram[BIOS_TRACK_TABLE + slot * 2 + 1] =
            static_cast<uint8_t>(((seconds / 10) << 4) | (seconds % 10));
    };

    for (const CdromToc::Entry& entry : neocd->cdrom.toc().toc())
    {
        uint8_t track = entry.trackIndex.track();

        if ((entry.trackIndex.index() != 1) || !track || (track > 99))
            continue;

        write(track, entry.startSector);

        if (track > highest)
            highest = track;


    }

    // The slot after the last track holds where the disc runs out, which
    // is what a length is worked out against.
    if (highest && (highest < 99))
        write(static_cast<uint8_t>(highest + 1), neocd->cdrom.leadout());

    // And two things a BIOS learns while reading a disc: the last track
    // on it and the time it runs out at. It picks those up working
    // through the table of contents, which does not happen here - the
    // contents are simply known - so they were left at zero, and a game
    // asking about the disc was told it has no tracks and ends at
    // nothing.
    //
    // The byte beside them, 0x10F649, is left alone. It reads 01 on two
    // discs here and 00 on a third, steadily, so whatever it holds it is
    // not simply the first track number, and a guess at it would be
    // wrong on that third disc.
    if (highest)
    {
        uint8_t* ram = neocd->memory.ram;
        uint32_t frames = neocd->cdrom.leadout() + 150;
        uint32_t seconds = frames / 75;
        uint32_t minutes = seconds / 60;

        seconds %= 60;

        ram[0x10F642] = static_cast<uint8_t>(((minutes / 10) << 4) | (minutes % 10));
        ram[0x10F643] = static_cast<uint8_t>(((seconds / 10) << 4) | (seconds % 10));
        ram[0x10F64A] = static_cast<uint8_t>(((highest / 10) << 4) | (highest % 10));
    }
}

void HleBios::initBiosRam()
{
    // The area above 0x10F300 is the BIOS's own, and games read it
    // directly rather than asking for its contents.
    std::memset(neocd->memory.ram + BIOS_RAM_START, 0, 0x10FFFF - BIOS_RAM_START + 1);

    uint8_t* ram = neocd->memory.ram;

    // A game polls this before going on. A real BIOS has it set for as
    // long as a game is running - it reads zero only while the BIOS
    // still has the machine, which is never the case here.
    ram[0x10F6D9] = 0x01;

    // Read by games and holding the same values whichever game is
    // running, so these are the BIOS's own and are set here.
    ram[0x10F781] = 0x01;
    ram[0x10F785] = 0x49;

    // The settings block a BIOS sets up. Which of these are its own was
    // settled by reading them under two different games: the ones below
    // hold the same value whichever game is running, and the ones beside
    // them - 0x10FD88, 0x10FD8A, 0x10FD8B, 0x10FD8D, 0x10FD8F, 0x10FD91 -
    // do not, so those belong to the game and are left alone. Seeding
    // them from one game's run, which is what this used to do, sent
    // another game down a path it never takes.
    ram[0x10FD81] = 0x02;

    // A BIOS works out where the block it hands the CD controller lives
    // and writes the address here. A game can name its own in its header
    // - a word at 0x13A, doubled and taken from the upload window - and
    // one that does not gets the address a BIOS falls back on.
    //
    // Nothing wrote this, so it read as zero, and the routine that
    // answers a game asking for a track checks it before doing anything:
    // no block, no command, no music.
    {
        // The word at 0x13A can name a game's own block, but a BIOS only
        // honours it for one mode of a call this does not model - left
        // alone, every game measured gets the default. Samurai Shodown 2
        // carries 0x8643 there and a real BIOS still uses the default;
        // taking the header at face value pointed this at a stretch of
        // the sound driver's own code, and the requests the driver was
        // posting at the real block went unread.
        uint32_t block = CD_COMMAND_BLOCK;

        ram[BIOS_CD_COMMAND]     = static_cast<uint8_t>(block >> 24);
        ram[BIOS_CD_COMMAND + 1] = static_cast<uint8_t>(block >> 16);
        ram[BIOS_CD_COMMAND + 2] = static_cast<uint8_t>(block >> 8);
        ram[BIOS_CD_COMMAND + 3] = static_cast<uint8_t>(block);

        // And the byte beside it says the CD side is idle and listening.
        // A real BIOS parks FF here between requests; a game that reads
        // it before asking sees zero from this and never asks.
        ram[CD_MODE_VAR] = 0xFF;
    }

    // The settings a game ships for the country it is running in. A
    // BIOS picks a pointer out of the game's own header, four bytes per
    // country, steps sixteen bytes into what it points at and copies a
    // block from there: six bytes as they stand, then ten more with each
    // one shifted down a nibble.
    //
    // These were being seeded with values watched in one running game,
    // which is how another game came to read three where it should read
    // zero and take a branch it never takes on hardware. They belong to
    // the game and this is where a BIOS gets them.
    {
        uint32_t table = 0x000116 + (ram[BIOS_COUNTRY] * 4);
        uint32_t source = m68k_read_memory_32(table) + 0x10;

        for (uint32_t k = 0; k < 6; ++k)
            ram[0x10FD84 + k] = static_cast<uint8_t>(m68k_read_memory_8(source + k));

        for (uint32_t k = 0; k < 10; ++k)
            ram[0x10FD8A + k] =
                static_cast<uint8_t>(m68k_read_memory_8(source + 6 + k) >> 4);
    }

    // What is deliberately not set is anything a game writes for
    // itself. 0x10FDC6 is the example that cost a while: watched
    // mid-game it holds 0x81 under one title and 0x00 under another,
    // and seeding it with the first sent the second down a path it
    // never takes on hardware.

    ram[BIOS_MVS_FLAG]  = 0x00;   // a home console, not an arcade board
    ram[BIOS_COUNTRY]   = 0x00;   // Japan
    ram[BIOS_SYSTEM_MODE] = 0x00;   /* a game says what mode it is in; set per request in callUser */
    // Not zeroed here: 0x10FD84 is the first byte of the block a game
    // supplies above, and clearing it threw that byte away again.

    // The sound CPU stays in reset here. A BIOS holds it down while it
    // loads and lets it go on its way into the game, which is where
    // this lets it go too.
    neocd->z80Disable = true;
    neocd->z80NMIDisable = false;

    // The display is off while a real BIOS is loading and it turns it
    // back on before handing over. A game does not do that for itself,
    // so without this everything below draws into a blanked screen.
    neocd->video.videoEnable = true;
    neocd->video.sprDisable = false;
    neocd->video.fixDisable = false;

    // The interrupt masks are hardware registers the real BIOS writes
    // while starting up, not something a game sets for itself. Without
    // them the vertical blank never fires and a game sits waiting for a
    // frame that does not arrive.
    neocd->irqMask1 = 0x50;
    neocd->irqMask2 = 0x30;
}

void HleBios::repeatPad(uint32_t base, uint8_t current)
{
    uint8_t* ram = neocd->memory.ram;

    if (ram[base + 2] != current)
    {
        ram[base + 5] = 0x10;
        ram[base + 4] = current;
    }
    else if (ram[base + 5]--)
    {
        ram[base + 4] = 0x00;
    }
    else
    {
        ram[base + 5] = 0x08;
        ram[base + 4] = current;
    }
}

void HleBios::cdRequest(uint8_t mode, uint8_t track, bool fromBlock)
{
    uint8_t* ram = neocd->memory.ram;

    ram[CD_MODE_VAR] = mode;

    if (!(mode & 0x02))
    {
        ram[CD_TRACK_VAR] = track;
        ram[0x10F6F8] = track;
        ram[0x10F6F7] = mode;
    }
    else
    {
        // Keep the track already asked for.
        track = ram[CD_TRACK_VAR];

        // Of the modes that name no track, the lower pair stop the
        // drive and the upper pair leave it be - which is what lets a
        // resume find the track still stored.
        if (!(mode & 0x04))
        {
            neocd->cdrom.stop();
            m_playUntil = 0;
            return;
        }
    }

    if (!track)
        return;

    // The track is held the way it is written down, two decimal
    // digits to a byte, not as a plain count.
    uint8_t number = static_cast<uint8_t>(((track >> 4) * 10) + (track & 0x0F));

    const CdromToc::Entry* entry =
        neocd->cdrom.toc().findTocEntry(TrackIndex(number, 1));

    if (entry && (entry->trackType != CdromToc::TrackType::Mode1_2048)
              && (entry->trackType != CdromToc::TrackType::Mode1_2352))
    {
        // Where this track runs out: the next one's start, or the
        // end of the disc for the last. A drive stops there.
        const CdromToc::Entry* next =
            neocd->cdrom.toc().findTocEntry(TrackIndex(number + 1, 1));

        m_playUntil = next ? next->startSector : neocd->cdrom.leadout();
        m_loopAtEnd = fromBlock;

        // Asking again for the track already playing leaves it
        // alone rather than starting it over.
        if (!neocd->cdrom.isPlaying()
            || (neocd->cdrom.currentTrackIndex().track() != number))
        {
            neocd->cdrom.seek(entry->startSector);
            neocd->cdrom.play();
        }
    }
}

void HleBios::consumeCdBlock()
{
    // A game does not always ask for music itself. Samurai Shodown 2
    // never does: its 68000 sends song numbers to the sound driver like
    // any other sound, and it is the driver, on the Z80, that knows
    // which of them are disc tracks. The driver posts those on in its
    // own memory, and the block address a BIOS keeps at 0x10F6EA names
    // that spot as the 68000 sees it - through the window whose odd
    // bytes are the Z80's memory at half the offset. 0xE1FDF0 is Z80
    // 0xFEF8: the command, with the track beside it.
    //
    // The system call a game makes every frame reads the pair, acts and
    // clears it. Nothing here did, so the driver's requests sat there
    // unread - the pair of bytes an earlier investigation found holding
    // 02 02 against a real run's 00 00 and took for debris was the
    // request itself, a stop the driver posts when it resets, waiting
    // forever to be taken.
    uint8_t* ram = neocd->memory.ram;

    uint32_t p = (static_cast<uint32_t>(ram[0x10F6EA]) << 24)
               | (static_cast<uint32_t>(ram[0x10F6EB]) << 16)
               | (static_cast<uint32_t>(ram[0x10F6EC]) << 8)
               | ram[0x10F6ED];

    if ((p < 0x00E00000) || (p > 0x00EFFFFF))
        return;

    uint32_t at = (p >> 1) & 0xFFFF;
    uint8_t command = neocd->memory.z80Ram[at];
    uint8_t track = neocd->memory.z80Ram[at + 1];

    // Nothing waiting. A command of zero is a real request when a track
    // rides with it - the driver's own table pairs plenty of tracks
    // with mode zero - so idle is only the two together.
    if ((command == 0xFF) || (!command && !track))
        return;

    if (!(command & 0x02) && !track)
        return;

    neocd->memory.z80Ram[at] = 0x00;
    neocd->memory.z80Ram[at + 1] = 0x00;

    // The top bit only marks the request as fresh; the mode is the rest,
    // and a real BIOS ignores anything past seven.
    command &= 0x7F;

    if (command <= 0x07)
        cdRequest(command, track, true);
}

void HleBios::stopAtTrackEnd()
{
    if (!m_playUntil || !neocd->cdrom.isPlaying())
        return;

    if (neocd->cdrom.position() < m_playUntil)
        return;

    // What happens when the track runs out depends on how it was asked
    // for - not on the mode bits, which a real BIOS's monitor never
    // consults for this. Requests that come in through the command
    // block arm the monitor, and the monitor's answer to a track
    // reaching its boundary is to seek back and play it again, every
    // time; the earlier reading of the measurements took the drive
    // slipping into the next track - a race the monitor sometimes
    // loses - for a mode-dependent choice. Plays asked for through the
    // direct entry never arm the monitor at all, and the drive simply
    // runs on into whatever follows, which for the discs that use it
    // is silence; that is the stop the measurements saw, and it is the
    // track's doing, not the mode's.
    uint8_t* ram = neocd->memory.ram;

    if (m_loopAtEnd)
    {
        uint8_t track = ram[CD_TRACK_VAR];
        uint8_t number = static_cast<uint8_t>(((track >> 4) * 10) + (track & 0x0F));

        const CdromToc::Entry* entry =
            neocd->cdrom.toc().findTocEntry(TrackIndex(number, 1));

        if (entry)
        {
            neocd->cdrom.seek(entry->startSector);
            neocd->cdrom.play();
            return;
        }
    }

    neocd->cdrom.stop();
    m_playUntil = 0;
}

void HleBios::pollInput()
{
    // Where a real BIOS leaves the pads, established by holding one
    // button at a time and watching which bytes move rather than by
    // assuming the layout. The first byte of each pair is what is held
    // now; the second is what a game reads to see it. Start and select
    // share a byte of their own, one bit per player.
    uint8_t* ram = neocd->memory.ram;

    // The hardware reports a pressed button as a zero bit; what the
    // BIOS leaves in RAM is the other way round.
    uint8_t p1 = static_cast<uint8_t>(~neocd->input.input1);
    uint8_t p2 = static_cast<uint8_t>(~neocd->input.input2);
    // Only the low four bits of the third port are start and select;
    // the rest is not the BIOS's to report and a real one leaves them
    // clear.
    uint8_t st = static_cast<uint8_t>(~neocd->input.input3) & 0x0F;

    uint8_t p1Change = static_cast<uint8_t>(p1 & ~m_lastP1);
    uint8_t p2Change = static_cast<uint8_t>(p2 & ~m_lastP2);
    uint8_t stChange = static_cast<uint8_t>(st & ~m_lastStatus);
    m_startLatch |= stChange;

    ram[BIOS_P1PREVIOUS] = m_lastP1;
    ram[BIOS_P1CURRENT]  = p1;
    ram[BIOS_P1CHANGE]   = p1Change;
    ram[BIOS_P2PREVIOUS] = m_lastP2;
    ram[BIOS_P2CURRENT]  = p2;
    ram[BIOS_P2CHANGE]   = p2Change;

    // What a game reads to walk a menu is not the edge, it is the byte a
    // BIOS keeps beside it: the buttons held, emitted when they change
    // and then again on a beat while they stay down. Sixteen frames
    // before the first repeat, eight between the rest, and nothing at
    // all in between. Neither that byte nor the counter behind it was
    // written here, so a held direction moved a cursor once and never
    // again - and games that only read that byte saw nothing at all.
    repeatPad(BIOS_P1BASE, p1);
    repeatPad(BIOS_P2BASE, p2);

    // Start and select, both players: bit 0 and 1 are player one's
    // start and select, bits 2 and 3 player two's.
    ram[BIOS_STATCURNT]  = st;
    ram[BIOS_STATCHANGE] = stChange;

    m_lastP1 = p1;
    m_lastP2 = p2;
    m_lastStatus = st;
}

void HleBios::callUser(uint8_t request)
{
    uint8_t* ram = neocd->memory.ram;

    // A BIOS sets more than the request before it hands over. Asking for
    // the game proper also means saying so in the mode byte and clearing
    // the two longs beside it, which is state a game reads back and was
    // being left at zero here.
    // Let the sound CPU go. A BIOS does this on its way in, after the
    // program a game loaded is in place - starting it earlier leaves it
    // running whatever was in that memory before.
    if (request == 2)
    {
        neocd->z80Disable = false;
        z80_reset();
        YM2610Reset();
    }

    // Anything pressed before now was pressed while a real machine would
    // still have been reading the disc, with no game running to see it.
    // Here the disc is read in no time, so those presses would otherwise
    // arrive the instant the game starts and start it again - which is
    // what King of Fighters '99 was doing to itself, going in-game
    // before its intro and sitting there.
    m_startLatch = 0;

    // The mode byte carries the request with the top bit set.
    ram[BIOS_SYSTEM_MODE] = static_cast<uint8_t>(request | 0x80);

    if (request == 2)
    {
        ram[BIOS_USER_MODE] = 0x01;
        ram[0x10FEE1] = 0x0A;
        ram[0x10F675] = 0x01;
        for (uint32_t i = 0; i < 4; ++i)
        {
            ram[0x10FDB6 + i] = 0x00;
            ram[0x10FDBA + i] = 0x00;
        }
    }

    ram[BIOS_USER_REQUEST] = request;

    // Entered with the stack exactly where a real BIOS leaves it. That
    // matters: a game gives control back by jumping to SYSTEM_RETURN
    // rather than returning, so it is not expecting a return address to
    // have been pushed, and pushing one leaves its stack four bytes out
    // for as long as it runs.
    //
    // Somewhere to land is still wanted for the games that do return,
    // so the address goes at the top of the stack rather than below it:
    // the stack pointer reads the same as on hardware, and a return
    // finds something better than address zero.
    m68k_write_memory_32(0x0010F300, USER_RETURN);
    m68k_set_reg(M68K_REG_SP, 0x0010F300);

    // Entered with interrupts masked. A game opens them itself once it
    // has its handlers in place; letting a vertical blank in before
    // that means running its handler before it is ready for one.
    m68k_set_reg(M68K_REG_SR, 0x2700);

    m68k_set_reg(M68K_REG_PC, USER_VECTOR);
}

int HleBios::trap(uint32_t pc)
{
    if (getenv("ENTLOG"))
    {
        static unsigned seen[64]; static unsigned nseen = 0;
        bool has = false;
        for (unsigned q = 0; q < nseen; ++q) if (seen[q] == pc) { has = true; break; }
        if (!has && nseen < 64)
        {
            seen[nseen++] = pc;
            fprintf(stderr, "entry %06X\n", pc);
        }
    }

    switch (pc)
    {
    case BOOT:
        if (!loadDisc())
        {
            // Nothing to run. Stop rather than execute the disc's
            // contents as if they were code.
            m68k_set_reg(M68K_REG_PC, IGNORE_IRQ);
            return 1;
        }

        initBiosRam();
        buildTrackTable();
        neocd->memory.mapVectorsToRam();
        m_booted = true;

        Libretro::Log::message(RETRO_LOG_INFO, "HLE BIOS: disc loaded, entering the game.\n");

        // Request 0 is the game's own initialisation.
        m_userRequest = 0;
        callUser(0);
        return 1;

    case USER_RETURN:
    case SYSTEM_RETURN:
        // The game has finished what it was asked to do. The real BIOS
        // decides what comes next; with no coin handling or service
        // menu here, that is the game itself, over and over.
        if (m_userRequest == 0)
            m_userRequest = 2;   // startup done, run the game

        // Do not go straight back in. A BIOS clears the top bit of the
        // mode byte and carries on in its own loop with interrupts
        // open; the game's handler sees that bit clear and ends the
        // interrupt without doing its frame's work. Only then does the
        // BIOS enter the game again, setting the bit as it goes.
        neocd->memory.ram[BIOS_SYSTEM_MODE] &= 0x7F;

        // A BIOS waits four frames here, not none. It has a routine
        // that sets a flag and spins until the frame interrupt clears
        // it, and the path between a hand-back and the next entry calls
        // that routine four times.
        m_userDelay = 5;
        m68k_set_reg(M68K_REG_SP, 0x0010F300);
        m68k_set_reg(M68K_REG_SR, 0x2000);
        m68k_set_reg(M68K_REG_PC, IDLE);
        return 1;

    case VBLANK:
        pollInput();
        {
            uint8_t* ram = neocd->memory.ram;
            uint32_t frame = (static_cast<uint32_t>(ram[BIOS_FRAME_COUNTER]) << 24)
                           | (static_cast<uint32_t>(ram[BIOS_FRAME_COUNTER + 1]) << 16)
                           | (static_cast<uint32_t>(ram[BIOS_FRAME_COUNTER + 2]) << 8)
                           | static_cast<uint32_t>(ram[BIOS_FRAME_COUNTER + 3]);
            ++frame;
            ram[BIOS_FRAME_COUNTER]     = static_cast<uint8_t>(frame >> 24);
            ram[BIOS_FRAME_COUNTER + 1] = static_cast<uint8_t>(frame >> 16);
            ram[BIOS_FRAME_COUNTER + 2] = static_cast<uint8_t>(frame >> 8);
            ram[BIOS_FRAME_COUNTER + 3] = static_cast<uint8_t>(frame);
        }
        neocd->clearInterrupt(NeoGeoCD::VerticalBlank);
        neocd->updateInterrupts();
        return 1;

    case IGNORE_IRQ:
        neocd->clearInterrupt(NeoGeoCD::CdromDecoder);
        neocd->clearInterrupt(NeoGeoCD::CdromCommunication);
        neocd->clearInterrupt(NeoGeoCD::Raster);
        neocd->updateInterrupts();
        return 1;

    case CD_PLAY_TRACK:
        // The music. A game names a track in the low byte of D0 and what
        // to do with it in the high byte.
        //
        // Bit 1 of that does not mean stop, which is what this had it
        // meaning and why King of Fighters '99 fell silent the moment it
        // asked for music: a BIOS tests the same bit to decide whether
        // to take the track from D0 at all, so with it set the track
        // already asked for stands and only the mode is new. Reading it
        // as stop turned every one of those into silence.
        {
            uint32_t d0 = m68k_get_reg(nullptr, M68K_REG_D0);

            cdRequest(static_cast<uint8_t>((d0 >> 8) & 0xFF),
                      static_cast<uint8_t>(d0 & 0xFF));
        }
        return 1;

    case CD_QUIET_1:
    case CD_QUIET_2:
        return 1;

    case UPLOAD:
        // A queued upload, read out of a BIOS rather than guessed at.
        // The type at 0x10FEDA picks which area it lands in and the
        // three longs beside it say where from, where to and how much:
        //
        //   0x10FEF8  source, in the game's own memory
        //   0x10FEF4  destination, an offset into the area
        //   0x10FEFC  length in bytes
        //   0x10FEDB  which bank, for the areas that have them
        //
        // A BIOS selects the area, hands the three to the transfer
        // hardware and lets it run. The same thing happens here by
        // writing through the upload window at 0xE00000, which is what
        // that hardware writes through - so the area select, the bank
        // and the halved addressing the sample memory wants are all
        // applied by the same code that applies them to a game.
        //
        // This used to set four bytes watched in a real BIOS run and
        // move nothing at all.
        {
            uint8_t* ram = neocd->memory.ram;
            uint8_t type = static_cast<uint8_t>(ram[0x10FEDA] & 0x0F);

            uint32_t source = m68k_read_memory_32(0x0010FEF8);
            uint32_t destination = m68k_read_memory_32(0x0010FEF4);
            uint32_t length = m68k_read_memory_32(0x0010FEFC);

            fprintf(stderr, "HLE upload: type=%u bank=%u src=%06X dst=%06X len=%06X\n",
                type, ram[0x10FEDB], source, destination, length);

            // Which area, the bank register that goes with it - and the
            // bus. The window only takes writes for an area whose bus
            // has been asked for, which is what the request registers
            // are for; a real BIOS asks before copying and releases
            // after. Without asking, every byte of a game's upload fell
            // into the gate and vanished - King of Fighters '99 uploads
            // its characters' sample maps to the sound driver this way,
            // and the driver keyed a silent placeholder for every hit
            // because the map never arrived.
            uint32_t request = 0;
            uint32_t release = 0;

            switch (type)
            {
            case 1:  m68k_write_memory_8(0x00FF0105, 5);
                     request = 0x00FF0129; release = 0x00FF0149; break;
            case 2:  m68k_write_memory_8(0x00FF0105, 0);
                     m68k_write_memory_8(0x00FF01A1, ram[0x10FEDB]);
                     request = 0x00FF0121; release = 0x00FF0141; break;
            case 3:  m68k_write_memory_8(0x00FF0105, 4);
                     request = 0x00FF0127; release = 0x00FF0147; break;
            case 4:  m68k_write_memory_8(0x00FF0105, 1);
                     m68k_write_memory_8(0x00FF01A3, ram[0x10FEDB]);
                     request = 0x00FF0123; release = 0x00FF0143; break;
            default: break;
            }

            if (request)
                m68k_write_memory_8(request, 1);

            // Type five is not a copy at all: it is the sample map for
            // the sound driver, read out of a real BIOS. The source
            // holds six byte records - a spot in the driver, a start
            // and an end - and the destination names where in sample
            // memory the sounds were put, bank and all. What lands in
            // the driver is the same arithmetic the .PAT files use:
            // add the base, halve, and make the end inclusive. King of
            // Fighters '99 sends its characters' maps this way after
            // every load; treated as a plain copy they scribbled over
            // whatever area was last selected, the driver's tables
            // kept their authored placeholder, and every hit in a
            // battle played the same silent tick.
            // Type zero is program memory: the transfer hardware copies
            // it straight from one place in the 68000's memory to
            // another, no window involved. Left to the window loop it
            // went through whatever area was last selected instead.
            if (type == 0)
            {
                for (uint32_t done = 0; (done + 1) < length; done += 2)
                    m68k_write_memory_16(destination + done,
                                         m68k_read_memory_16(source + done));

                m68k_write_memory_32(0x0010FEF8, source + length);
                m68k_write_memory_32(0x0010FEF4, destination + length);
                m68k_write_memory_32(0x0010FEFC, 0);
                return 1;
            }

            if (type == 5)
            {
                // The records are the .PAT format to the byte: ten each,
                // a spot in the driver, a start and an end, then a
                // second pair that goes five bytes past the first when
                // it is not zeroes. A six byte reading of them took each
                // record's empty second pair for the end of the list,
                // so of a character's whole map one record in each
                // upload landed - which is why a battle had exactly one
                // effect, the landing thud, and nothing else.
                uint32_t base = ((static_cast<uint32_t>(ram[0x10FEDB] & 1) << 20)
                              | (destination & 0xFFFFF)) >> 8;

                for (uint32_t at = 0; (at + 10) <= length; at += 10)
                {
                    uint32_t where = m68k_read_memory_16(source + at);

                    if ((where == 0x0000) || (where == 0xFFFF))
                        continue;

                    for (int half = 0; half < 2; ++half)
                    {
                        uint32_t f = at + 2 + (half * 4);
                        uint32_t start = m68k_read_memory_16(source + f);
                        uint32_t end   = m68k_read_memory_16(source + f + 2);
                        uint32_t spot = where + (half * 5);

                        if (half && !start && !end)
                            break;

                        uint32_t startValue = (start + base) >> 1;
                        uint32_t endValue = (end + base) >> 1;

                        if (endValue)
                            endValue--;

                        if ((spot + 3) < Memory::Z80RAM_SIZE)
                        {
                            neocd->memory.z80Ram[spot]     = static_cast<uint8_t>(startValue);
                            neocd->memory.z80Ram[spot + 1] = static_cast<uint8_t>(startValue >> 8);
                            neocd->memory.z80Ram[spot + 2] = static_cast<uint8_t>(endValue);
                            neocd->memory.z80Ram[spot + 3] = static_cast<uint8_t>(endValue >> 8);
                        }
                    }
                }

                m68k_write_memory_32(0x0010FEFC, 0);
                return 1;
            }
            // How the copy goes depends on the area, read out of the two
            // transfer programs a real BIOS hands its DMA engine. The
            // byte areas - fix, sound program, samples - take one source
            // byte per slot, packed, in source order, and the window
            // position moves two for every byte; a word loop through the
            // window dropped every other byte here, which is why the
            // title text this BIOS finally showed came out as rubble.
            // Sprites and program move as words.
            if ((type == 1) || (type == 3) || (type == 4))
            {
                uint32_t bank = ram[0x10FEDB];

                for (uint32_t done = 0; done < length; ++done)
                {
                    uint32_t at = (destination >> 1) + done;
                    uint8_t value = static_cast<uint8_t>(m68k_read_memory_8(source + done));

                    switch (type)
                    {
                    case 1:
                        neocd->memory.fixRam[at & 0x1FFFF] = value;
                        if (value)
                            neocd->video.fixUsageMap[(at & 0x1FFFF) >> 5] = 1;
                        break;
                    case 3:
                        if (at < Memory::Z80RAM_SIZE)
                            neocd->memory.z80Ram[at] = value;
                        break;
                    case 4:
                        neocd->memory.pcmRam[(at + ((bank & 1) * 0x80000)) & 0xFFFFF] = value;
                        break;
                    }
                }

                uint32_t linear = ((bank & 1) * 0x100000) + destination + (length * 2);

                if (type == 4)
                {
                    ram[0x10FEDB] = static_cast<uint8_t>(linear >> 20);
                    m68k_write_memory_8(0x00FF01A3, ram[0x10FEDB]);
                }

                m68k_write_memory_32(0x0010FEF8, source + length);
                m68k_write_memory_32(0x0010FEF4, (type == 4) ? (linear & 0xFFFFF)
                                                             : (destination + (length * 2)));
                m68k_write_memory_32(0x0010FEFC, 0);
            }
            else
            {
                // Sprite and anything unnamed: a word at a time through
                // the window, stepping the bank when the destination
                // runs past the end of one.
                uint8_t bank = ram[0x10FEDB];

                for (uint32_t done = 0; (done + 1) < length; done += 2)
                {
                    if (destination >= 0x100000)
                    {
                        destination -= 0x100000;
                        ++bank;
                        ram[0x10FEDB] = bank;
                        if (type == 2) m68k_write_memory_8(0x00FF01A1, bank);
                    }

                    m68k_write_memory_16(0x00E00000 + destination,
                                         m68k_read_memory_16(source));
                    source += 2;
                    destination += 2;
                }

                m68k_write_memory_32(0x0010FEF8, source);
                m68k_write_memory_32(0x0010FEF4, destination);
                m68k_write_memory_32(0x0010FEFC, 0);
            }

            if (release)
                m68k_write_memory_8(release, 1);
        }
        return 1;

    case CLEAR_SPRITES:
        // Read out of a BIOS rather than guessed at. It sets the video
        // write step to one and fills the three sprite control blocks,
        // 512 entries each: no shrink, no vertical size, and an X that
        // puts every sprite off the right of the screen. A game calls it
        // to clear what is on screen before drawing something else.
        //
        // This used to answer with a register value watched once and
        // nothing else, so the sprites a game expected to be swept away
        // stayed where they were.
        {
            uint16_t* vram = neocd->memory.videoRam;

            for (uint32_t i = 0; i < 0x200; ++i)
            {
                vram[0x8000 + i] = 0x0FFF;
                vram[0x8200 + i] = 0x0000;
                vram[0x8400 + i] = 0xB000;
            }

            // Left as the routine leaves them: the step it set, and the
            // address just past the last block it wrote.
            neocd->video.videoramModulo = 1;
            neocd->video.videoramOffset = 0x8600;
        }
        return 1;

    case SOUND_COMMAND:
        // A game asking for a sound. A BIOS puts the byte in a queue and
        // sends it on as it works through the frame; nothing here needs
        // the queue, so it goes straight to the sound CPU. Without this
        // a game that asks for its music this way is answered with
        // nothing at all.
        m68k_write_memory_8(0x00320000,
            static_cast<uint8_t>(m68k_get_reg(nullptr, M68K_REG_D0) & 0xFF));
        return 1;

    case CD_STREAM_START:
    case CD_STREAM_ALT:
        streamFiles(m68k_get_reg(nullptr, M68K_REG_A0));

        // The state a BIOS establishes here, without the streaming it
        // goes on to do. A game that reaches this and gets no answer
        // stops; one that gets the flags carries on, though whatever it
        // expected to be streamed will not arrive.
        {
            uint8_t* ram = neocd->memory.ram;
            ram[0x10FDDC] = 0x01;
            ram[0x10FDDD] = 0x00;
            ram[0x10FE88] = 0x00;
            ram[0x10F6DB] = 0x01;
            ram[0x10FEC4] = 0x01;
            for (uint32_t i = 0; i < 4; ++i)
            {
                ram[0x10F742 + i] = 0x00;
                ram[0x10F746 + i] = 0x00;
            }
        }
        return 1;

    case CLEAR_TEXT:
        // Blanks the text layer, read out of a BIOS. Write step of one,
        // then from 0x701E: 1216 entries of 0x00FF, 32 more of 0x0020,
        // and another 32 of 0x0020 back at 0x6FFE. A game calls it to
        // wipe what is written across the screen.
        //
        // This used to work through the CD interface registers, which is
        // not what the routine does, and left the text where it was.
        {
            uint16_t* vram = neocd->memory.videoRam;
            uint32_t at = 0x701E;

            for (uint32_t i = 0; i < 0x4C0; ++i)
                vram[(at + i) & 0xFFFF] = 0x00FF;

            at += 0x4C0;

            for (uint32_t i = 0; i < 0x20; ++i)
                vram[(at + i) & 0xFFFF] = 0x0020;

            for (uint32_t i = 0; i < 0x20; ++i)
                vram[(0x6FFE + i) & 0xFFFF] = 0x0020;

            neocd->video.videoramModulo = 1;
            neocd->video.videoramOffset = (0x6FFE + 0x20) & 0xFFFF;
        }
        return 1;

    case SYSTEM_IO:
        pollInput();
        stopAtTrackEnd();
        consumeCdBlock();

        // Wound down here as well as in the frame interrupt: a game
        // calls one or the other once a frame, and which one differs
        // between games.
        if (m_busyFrames)
            --m_busyFrames;


        // Starting a game is a BIOS's job, not something a game does off
        // the pad for itself. On a console there is no credit to weigh
        // up: start goes down, a BIOS says how many are playing and
        // calls the game's own start entry. Without that a game sits at
        // its title however long the button is held.
        //
        // Here rather than in the frame interrupt because this is what a
        // game actually calls: Metal Slug 2 comes through here once a
        // frame and reaches SYSTEM_INT1 nineteen times in three thousand
        // frames.
        // Starting a game is a BIOS's job, but not on request alone: it
        // weighs four things first and this weighed none of them, so a
        // game was started the instant a button moved and put into its
        // in-game state before it was ready. King of Fighters '99 was
        // reaching its start entry twenty-eight times sooner than a real
        // BIOS takes it there, going in-game and sitting on its logo.
        //
        //   0x10F67A  must be zero, or something else is going on
        //   0x10FDAF  must not be, or no game has been entered yet
        //   0x10F6D9  must not be, or the machine is not ready
        //   the level 2 vector, if a transfer holds it, not now
        //
        // Start is not the low bits of what changed either. A BIOS walks
        // the byte two bits at a time and keeps every other one - start
        // for each of four players, with select in between - so taking
        // the bottom two made player one's select a start of its own.
        {
        uint8_t* ram = neocd->memory.ram;
        uint8_t starts = 0;
        for (uint32_t bit = 0; bit < 4; ++bit)
            if (m_startLatch & (1u << (bit * 2)))
                starts |= static_cast<uint8_t>(1u << bit);

        if (starts
            && !m_busyFrames
            && !ram[0x10F67A] && !ram[0x10F67B]
            && ram[BIOS_USER_MODE] && ram[0x10F6D9]
            && (m68k_read_memory_32(0x00000068) != 0x00C0A518)
            && (m_userRequest == 2) && !m_userDelay)
        {
            ram[BIOS_PLAYER_MOD] = starts;
            m_startLatch = 0;

            // Called, not jumped to: what it returns to is the
            // instruction that returns from this call.
            uint32_t sp = m68k_get_reg(nullptr, M68K_REG_SP) - 4;
            m68k_write_memory_32(sp, SYSTEM_IO + 2);
            m68k_set_reg(M68K_REG_SP, sp);
            m68k_set_reg(M68K_REG_PC, PLAYER_START);
        }
        }
        return 1;

    case SYSTEM_INT1:
        // A game that handed the machine back is given it again here,
        // once its handler has been round once.
        // The frame flag a BIOS spins on is cleared here.
        neocd->memory.ram[0x10F6D8] = 0x00;

        stopAtTrackEnd();
        consumeCdBlock();

        if (m_busyFrames)
            --m_busyFrames;

        if (m_userDelay && !--m_userDelay)
        {
            pollInput();
            neocd->clearInterrupt(NeoGeoCD::VerticalBlank);
            neocd->updateInterrupts();
            callUser(m_userRequest);
            return 1;
        }

        // The real BIOS keeps a counter here, stepped once a frame.
        // Games seed their random numbers from it, and one that never
        // moves leaves them spinning in a loop waiting for a different
        // number - which is not a hang a game can be blamed for.
        ++neocd->memory.ram[0x10F749];

        // A real BIOS's CD service keeps a pair of heartbeat counters
        // at 0x10FE84 and 0x10FE85: the first steps on every pass, the
        // second steps when the disc position has moved on as expected
        // and is cleared when the first wraps, so while the drive is
        // reading or playing the two climb together and while it sits
        // idle both stand still. The King of Fighters '99 samples the
        // second one for eight frames during its transitions and, when
        // it never moves, declares DISK I/O ERROR ( ID = 0003 ) and
        // asks to be powered off. Here the drive is an illusion and
        // every operation goes perfectly, so the counters climb
        // together whenever the illusion would be busy: audio playing,
        // or a load still being paced out.
        if (neocd->cdrom.isPlaying() || m_busyFrames || m_heartbeatFrames)
        {
            uint8_t* hbram = neocd->memory.ram;

            if (m_heartbeatFrames)
                --m_heartbeatFrames;

            ++hbram[0x10FE84];
            if (!hbram[0x10FE84])
                hbram[0x10FE85] = 0;
            ++hbram[0x10FE85];
        }

        // Acknowledging the interrupt is the point of this call. Without
        // it the level stays asserted and the handler is re-entered the
        // instant it returns, which leaves a game running its interrupt
        // and never its main loop.
        pollInput();
        neocd->clearInterrupt(NeoGeoCD::VerticalBlank);
        neocd->updateInterrupts();
        return 1;

    case CD_IRQ_54:
    case CD_IRQ_58:
    case CD_IRQ_5C:
    case CD_IRQ_60:
        // The CD interrupts a game's level 2 handler hands on to.
        //
        // A real BIOS does work here: it polls the drive and leaves the
        // result in its own area - the position at 0x10F750, and flags
        // at 0x10F65B, 0x10F703 and 0x10F717. None of that is
        // reproduced yet, so this only acknowledges. The entries have
        // to exist regardless: without them a game that reaches one
        // stops dead.
        neocd->clearInterrupt(NeoGeoCD::CdromDecoder);
        neocd->clearInterrupt(NeoGeoCD::CdromCommunication);
        neocd->updateInterrupts();
        return 1;

    case SYSTEM_INT2:
        // Ending an interrupt, whichever one it was. A game reaches
        // this from its vertical blank handler as readily as from a CD
        // one, and leaving the blank asserted re-enters the handler the
        // instant it returns.
        neocd->clearInterrupt(NeoGeoCD::CdromDecoder);
        neocd->clearInterrupt(NeoGeoCD::CdromCommunication);
        neocd->clearInterrupt(NeoGeoCD::VerticalBlank);
        neocd->updateInterrupts();
        return 1;

    case FRAME_UPDATE:
        // Nothing to do on a console. The routine looks at the country
        // first, then at the arcade flag, and returns when that is clear
        // - everything past it is the coin and credit handling an arcade
        // board needs. What used to be here, a frame counter and the
        // pads' repeat timers winding down, was assembled from watching
        // bytes move in a real BIOS run and attributed to this routine
        // because it happened to be running.
        return 1;

    case CARD_ERROR:
        // Asks what went wrong with the card. A BIOS saves the stack
        // pointer it would unwind to, copies the card status somewhere a
        // game can read it back, and then decides: a status below 0x81,
        // or 0x86 and above, is nothing it handles, so it returns. There
        // is no card here and the status says so, which lands in the
        // first of those, so it returns.
        {
            uint8_t* ram = neocd->memory.ram;
            uint32_t sp = m68k_get_reg(nullptr, M68K_REG_SP);

            ram[0x10F3F6] = static_cast<uint8_t>(sp >> 24);
            ram[0x10F3F7] = static_cast<uint8_t>(sp >> 16);
            ram[0x10F3F8] = static_cast<uint8_t>(sp >> 8);
            ram[0x10F3F9] = static_cast<uint8_t>(sp);

            ram[0x10FDC7] = ram[0x10FDC6];
        }
        return 1;

    case CARD:
        // The memory card. A BIOS checks two bits of the status port for
        // one and, finding none, says so in 0x10FDC6 and returns -
        // everything past that point is card commands. There is no card
        // here, so this takes the same early exit rather than pretend to
        // one. It leaves the two things the routine sets before it
        // looks: a marker, and the stack pointer it would unwind to.
        {
            uint8_t* ram = neocd->memory.ram;
            uint32_t sp = m68k_get_reg(nullptr, M68K_REG_SP);
            ram[0x10F3F4] = 0xFF;
            ram[0x10F3F6] = static_cast<uint8_t>(sp >> 24);
            ram[0x10F3F7] = static_cast<uint8_t>(sp >> 16);
            ram[0x10F3F8] = static_cast<uint8_t>(sp >> 8);
            ram[0x10F3F9] = static_cast<uint8_t>(sp);
            ram[0x10FDC6] = 0x80;
        }
        return 1;

    case READ_CALENDAR:
    case SETUP_CALENDAR:
    case HOW_TO_PLAY:
    case CHECKSUM:
        // Nothing to implement: in a BIOS these slots reach a bare rts.
        // Answering them with a return is not a stand-in for the real
        // routine, it is the real routine.
        return 1;

    case CREDIT_DOWN:
        // Reaches a routine that looks at the arcade flag first and
        // returns when it is clear, which it is on a console.
        return 1;

    default:
        break;
    }

    // Anywhere else inside the BIOS is a routine that has not been
    // written. Naming it and stopping is more use than letting the
    // game fault into an exception vector, which ends as an endless
    // fault rather than a diagnosis.
    if ((pc >= ROM_BASE) && (pc < (ROM_BASE + ROM_SIZE)))
    {
        static bool reported = false;
        if (!reported)
        {
            reported = true;
            Libretro::Log::message(RETRO_LOG_ERROR,
                "HLE BIOS: the game called 0x%06X, which is not implemented. Stopping.\n", pc);
        }

        m68k_set_reg(M68K_REG_SP, 0x0010F300);
        m68k_set_reg(M68K_REG_SR, 0x2700);
        m68k_set_reg(M68K_REG_PC, HALT);
        return 1;
    }

    return 0;
}

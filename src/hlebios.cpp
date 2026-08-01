#include <cstring>
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

    entryPoint(rom, CD_SET_MODE, OP_RTS);
    entryPoint(rom, CD_QUIET_1, OP_RTS);
    entryPoint(rom, CD_UPLOAD, OP_RTS);
    entryPoint(rom, CD_WAIT, OP_RTS);
    entryPoint(rom, CD_QUIET_2, OP_RTS);
    entryPoint(rom, FRAME_UPDATE, OP_RTS);
    entryPoint(rom, CD_STATE_SET, OP_RTS);

    // The random number table. What matters is that the values are well
    // spread and unchanging, not what they are, so these are generated
    // rather than reproduced: a game indexes the table with a counter
    // and wants a different answer each time it looks.
    {
        uint32_t state = 0x2545F491;
        for (uint32_t i = 0; i < RANDOM_TABLE_SIZE; ++i)
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            rom[(RANDOM_TABLE - ROM_BASE) + i] = static_cast<uint8_t>(state >> 24);
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
        return false;

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

bool HleBios::findFile(const std::string& name, uint32_t& lba, uint32_t& size)
{
    // The root directory size comes straight from the volume descriptor,
    // which a malicious or corrupt disc controls. Reject an absurd value
    // before it becomes a huge allocation, and round the buffer up to a
    // whole number of sectors: readSector always writes a full 2048 bytes,
    // so a size that is not sector-aligned would overflow the vector on
    // the final sector.
    static const uint32_t MAX_ROOT_DIR_SIZE = 1u << 20;

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
        Libretro::Log::message(RETRO_LOG_ERROR, "HLE BIOS: %s is listed in IPL.TXT but not on the disc.\n", entry.name.c_str());
        return false;
    }

    std::vector<uint8_t> data;
    if (!readFile(lba, size, data))
        return false;

    // Which memory a file goes to is decided by its extension; the real
    // BIOS picks the upload target the same way.
    std::string ext;
    size_t dot = entry.name.rfind('.');
    if (dot != std::string::npos)
        ext = entry.name.substr(dot + 1);
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
        // The bank is not an address here. Boot lists pair a bank of 1
        // with offsets that already run the length of the area - SND_00
        // at 0 and SND_0C at 0x80000, both bank 1 - so the offset alone
        // addresses the megabyte and adding the bank would run past it.
        destination = neocd->memory.pcmRam;
        capacity = Memory::PCMRAM_SIZE;
    }
    else if (ext == "PAT")
    {
        // Palette data. It is kept as native words rather than the
        // stream's big-endian, and the video keeps a converted copy of
        // every colour, so this cannot be a plain copy like the rest.
        size_t colours = data.size() / 2;
        if (colours > (Memory::PALETTERAM_SIZE / 2))
            colours = Memory::PALETTERAM_SIZE / 2;

        for (size_t i = 0; i < colours; ++i)
        {
            uint16_t colour = static_cast<uint16_t>((data[i * 2] << 8) | data[i * 2 + 1]);
            neocd->memory.paletteRam[i] = colour;
            neocd->video.convertColor(static_cast<uint32_t>(i));
        }

        Libretro::Log::message(RETRO_LOG_INFO, "HLE BIOS: loaded %-16s %7zu bytes -> PAL\n",
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

    if ((offset >= capacity) || ((offset + data.size()) > capacity))
    {
        Libretro::Log::message(RETRO_LOG_ERROR,
            "HLE BIOS: %s does not fit at 0x%X (%zu bytes into %zu).\n",
            entry.name.c_str(), offset, data.size(), capacity);
        return false;
    }

    std::memcpy(destination + offset, data.data(), data.size());

    Libretro::Log::message(RETRO_LOG_INFO, "HLE BIOS: loaded %-16s %7zu bytes -> %s+0x%X\n",
        entry.name.c_str(), data.size(), ext.c_str(), offset);

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
    ram[0x10FD88] = 0x03;
    ram[0x10FD8D] = 0x01;
    ram[0x10FD91] = 0x02;

    // What is deliberately not set is anything a game writes for
    // itself. 0x10FDC6 is the example that cost a while: watched
    // mid-game it holds 0x81 under one title and 0x00 under another,
    // and seeding it with the first sent the second down a path it
    // never takes on hardware.

    ram[BIOS_MVS_FLAG]  = 0x00;   // a home console, not an arcade board
    ram[BIOS_COUNTRY]   = 0x00;   // Japan
    ram[BIOS_SYSTEM_MODE] = 0x82;
    ram[BIOS_GAME_DIP]  = 0x00;

    // The sound CPU is held in reset while a real BIOS loads and is
    // started before the game runs. A game with a Z80 program on the
    // disc expects it to be answering by then.
    neocd->z80Disable = false;
    neocd->z80NMIDisable = false;
    z80_reset();
    YM2610Reset();

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

    ram[BIOS_P1PREVIOUS] = m_lastP1;
    ram[BIOS_P1CURRENT]  = p1;
    ram[BIOS_P1CHANGE]   = p1Change;
    ram[BIOS_P2PREVIOUS] = m_lastP2;
    ram[BIOS_P2CURRENT]  = p2;
    ram[BIOS_P2CHANGE]   = p2Change;

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
    neocd->memory.ram[BIOS_USER_REQUEST] = request;

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

    // Supervisor, interrupts allowed: the vertical blank is what drives
    // a game once it is running.
    m68k_set_reg(M68K_REG_SR, 0x2000);

    m68k_set_reg(M68K_REG_PC, USER_VECTOR);
}

int HleBios::trap(uint32_t pc)
{
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

        callUser(m_userRequest);
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

    case CD_SET_MODE:
        // Watching a real BIOS: the high byte of D0 is stored here and
        // nothing else changes.
        neocd->memory.ram[CD_MODE_VAR] =
            static_cast<uint8_t>((m68k_get_reg(nullptr, M68K_REG_D0) >> 8) & 0xFF);
        return 1;

    case CD_QUIET_1:
    case CD_QUIET_2:
        return 1;

    case CD_STATE_SET:
        // Watched under a real BIOS: these four bytes, and nothing else
        // a game can see.
        neocd->memory.ram[0x10FDE4] = 0x00;
        neocd->memory.ram[0x10FDE5] = 0x80;
        neocd->memory.ram[0x10FEC3] = 0x86;
        neocd->memory.ram[0x10FEF7] = 0x80;
        return 1;

    case CD_WAIT:
        m68k_set_reg(M68K_REG_D0, 0x0000B000);
        return 1;

    case CD_UPLOAD:
        // Watched under a real BIOS: it works through the CD interface
        // registers and leaves these two set, returning 0x20. What it
        // moves is not reproduced here, so this is a placeholder that
        // lets a game past the call rather than an implementation.
        neocd->memory.ram[0x10FDB0] = 0x01;
        neocd->memory.ram[0x10FDB1] = 0x01;
        m68k_set_reg(M68K_REG_D0, 0x00000020);
        return 1;

    case SYSTEM_IO:
        pollInput();
        return 1;

    case SYSTEM_INT1:
        // The real BIOS keeps a counter here, stepped once a frame.
        // Games seed their random numbers from it, and one that never
        // moves leaves them spinning in a loop waiting for a different
        // number - which is not a hang a game can be blamed for.
        ++neocd->memory.ram[0x10F749];

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
        // Watched under a real BIOS: the frame counter moves on, the
        // pads' repeat timers wind down, and two flags are cleared.
        {
            uint8_t* ram = neocd->memory.ram;
            ++ram[0x10F749];
            for (uint32_t t : { BIOS_P1TIMER, BIOS_P2TIMER, BIOS_P1TIMER2, BIOS_P2TIMER2 })
                if (ram[t])
                    --ram[t];
            ram[0x10F680] = 0x00;
            ram[0x10F6C3] = 0x00;
        }
        return 1;

    case CARD_ERROR:
        // Left alone deliberately. Watched under one game it leaves a
        // pointer and a flag behind and answers 0xFFFF, and doing that
        // for every game turned out to send another one somewhere it
        // never goes on hardware - it drew nothing at all afterwards.
        // Whatever this routine really decides, it is not the same
        // answer every time it is asked.
        return 1;

    case CREDIT_DOWN:
    case READ_CALENDAR:
    case SETUP_CALENDAR:
    case CARD:
    case HOW_TO_PLAY:
    case CHECKSUM:
        // Reached but not implemented. Say so once per address so a
        // missing routine is visible rather than silent.
        {
            static uint32_t reported[32] = { 0 };
            static size_t reportedCount = 0;
            bool seen = false;
            for (size_t i = 0; i < reportedCount; ++i)
                if (reported[i] == pc) { seen = true; break; }
            if (!seen && (reportedCount < 32))
            {
                reported[reportedCount++] = pc;
                Libretro::Log::message(RETRO_LOG_WARN, "HLE BIOS: unimplemented call at 0x%06X\n", pc);
            }
        }
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

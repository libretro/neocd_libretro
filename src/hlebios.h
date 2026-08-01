#ifndef HLEBIOS_H
#define HLEBIOS_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

/**
 * A stand-in for the NeoGeo CD BIOS.
 *
 * The BIOS is 68000 code: it boots the disc, and then stays resident as
 * the system half of every game - the vertical blank handler, the
 * controller read, the routines a game calls to move data about. A core
 * without one has nothing to run.
 *
 * This synthesises a ROM whose entry points are illegal instructions,
 * and implements what those entry points did in C++ instead. Where the
 * real BIOS drives the CD hardware to load a file, this reads the file
 * and writes it where it belongs; where the real BIOS keeps state in
 * the area of RAM above 0x10F300, this keeps the same state at the same
 * addresses, because that is where games look for it.
 */
class HleBios
{
public:
    // Builds the stand-in into a 512KB ROM image.
    static void buildRom(uint8_t* rom);

    // Called by the CPU when it reaches one of the entry points. Returns
    // nonzero if the instruction was one of ours.
    static int trap(uint32_t pc);

    // True once the disc has been loaded and the game entered.
    static bool booted();

    static void reset();

    // Addresses of the entry points, inside the ROM region.
    static constexpr uint32_t ROM_BASE       = 0xC00000;
    static constexpr uint32_t ROM_SIZE       = 0x80000;

    static constexpr uint32_t BOOT           = 0xC00100;  // reset lands here
    static constexpr uint32_t VBLANK         = 0xC00120;  // level 1 autovector
    static constexpr uint32_t IGNORE_IRQ     = 0xC00140;  // everything else
    static constexpr uint32_t USER_RETURN    = 0xC00160;  // a USER call came back
    static constexpr uint32_t HALT           = 0xC00180;  // stopped after a fault

    // A table of 256 bytes games index to get a random number. It is
    // data, not code: the BIOS carries it and games reach straight into
    // the ROM for it.
    static constexpr uint32_t RANDOM_TABLE   = 0xC04200;
    static constexpr uint32_t RANDOM_TABLE_SIZE = 256;

    // The BIOS's exception handlers. These addresses are not guessed:
    // a game's own vector table points at them, which is how the layout
    // below was established.
    static constexpr uint32_t EXC_START      = 0xC00402;  // reset lands here
    static constexpr uint32_t EXC_BUS_ERROR  = 0xC00408;
    static constexpr uint32_t EXC_ADDR_ERROR = 0xC0040E;
    static constexpr uint32_t EXC_ILLEGAL    = 0xC00414;
    static constexpr uint32_t EXC_PRIVILEGE  = 0xC0041A;
    static constexpr uint32_t EXC_TRACE      = 0xC00420;
    static constexpr uint32_t EXC_SPURIOUS   = 0xC00426;
    static constexpr uint32_t EXC_UNINIT     = 0xC0042C;

    // The routines a game calls, which start after the exception
    // handlers. Each slot holds a JMP.L in the real BIOS, so they are
    // six bytes apart.
    static constexpr uint32_t SYSTEM_INT1    = 0xC00432;
    static constexpr uint32_t SYSTEM_INT2    = 0xC00438;
    static constexpr uint32_t SYSTEM_RETURN  = 0xC0043E;
    static constexpr uint32_t SYSTEM_IO      = 0xC00444;
    static constexpr uint32_t CREDIT_CHECK   = 0xC0044A;
    static constexpr uint32_t CREDIT_DOWN    = 0xC00450;
    static constexpr uint32_t READ_CALENDAR  = 0xC00456;
    static constexpr uint32_t SETUP_CALENDAR = 0xC0045C;
    static constexpr uint32_t CARD           = 0xC00462;
    static constexpr uint32_t CARD_ERROR     = 0xC00468;
    static constexpr uint32_t HOW_TO_PLAY    = 0xC0046E;
    static constexpr uint32_t CHECKSUM       = 0xC00474;

    // A CD BIOS routine, characterised by watching a real BIOS run it:
    // called with a mode in D0, it stores the high byte of D0 in a
    // variable and returns. Only observed with D0 = 0x0200.
    static constexpr uint32_t CD_SET_MODE    = 0xC0056A;
    static constexpr uint32_t CD_MODE_VAR    = 0x10F6F6;

    // Watched under a real BIOS and seen to change nothing a game can
    // observe: called with a mode in D0, returns, leaves BIOS RAM
    // alone. Stubbed on that basis rather than on a guess.
    static constexpr uint32_t CD_QUIET_1     = 0xC00570;

    // Works through the CD interface registers; only its effect on BIOS
    // RAM and its return value are reproduced, not what it transfers.
    static constexpr uint32_t CD_UPLOAD      = 0xC004C2;

    // Spends thousands of instructions and changes nothing any game can
    // see, which is what waiting for hardware looks like. Nothing here
    // is ever outstanding, so there is nothing to wait for.
    static constexpr uint32_t CD_WAIT        = 0xC004C8;

    // Thirteen instructions, no memory touched, D0 unchanged: a poke at
    // hardware and nothing a game can otherwise observe.
    static constexpr uint32_t CD_QUIET_2     = 0xC004CE;

    // Eighty-four instructions that set four bytes of BIOS state and
    // leave D0 alone. Reproduced by its effect.
    static constexpr uint32_t CD_STATE_SET   = 0xC00546;

    // The CD interrupt handlers, again taken from a game's vectors.
    static constexpr uint32_t CD_IRQ_54      = 0xC004F2;
    static constexpr uint32_t CD_IRQ_58      = 0xC004EC;
    static constexpr uint32_t CD_IRQ_5C      = 0xC004E6;
    static constexpr uint32_t CD_IRQ_60      = 0xC004E0;

    // TRAP #0 to #4.
    static constexpr uint32_t TRAP_BASE      = 0xC00522;

    // Where the BIOS keeps its state. Games read these directly.
    static constexpr uint32_t BIOS_RAM_START  = 0x10F300;
    static constexpr uint32_t BIOS_SYSTEM_MODE= 0x10FD80;
    // The request the BIOS is making of the game, and the mode the game
    // reports back. These two are adjacent and easy to transpose; the
    // request is the lower address.
    static constexpr uint32_t BIOS_USER_REQUEST = 0x10FDAE;
    static constexpr uint32_t BIOS_USER_MODE    = 0x10FDAF;
    static constexpr uint32_t BIOS_MVS_FLAG   = 0x10FD82;
    static constexpr uint32_t BIOS_COUNTRY    = 0x10FD83;
    static constexpr uint32_t BIOS_GAME_DIP   = 0x10FD84;
    static constexpr uint32_t BIOS_FRAME_COUNTER = 0x10FCF8;

    // The game's own entry points, in its header.
    static constexpr uint32_t USER_VECTOR     = 0x000122;

protected:
    struct IplEntry
    {
        std::string name;
        uint32_t bank;
        uint32_t offset;
    };

    static bool loadDisc();
    static bool readSector(uint32_t lba, uint8_t* out);
    static bool findFile(const std::string& name, uint32_t& lba, uint32_t& size);
    static bool readFile(uint32_t lba, uint32_t size, std::vector<uint8_t>& out);
    static bool parseIpl(const std::vector<uint8_t>& text, std::vector<IplEntry>& entries);
    static bool loadIplEntry(const IplEntry& entry);

    static void initBiosRam();
    static void callUser(uint8_t request);
    static void pollInput();

    static bool m_booted;
    static uint32_t m_rootLba;
    static uint32_t m_rootSize;
    static uint8_t m_userRequest;
};

#endif // HLEBIOS_H

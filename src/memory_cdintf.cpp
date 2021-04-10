#include "memory_cdintf.h"
#include "neogeocd.h"
extern "C" {
    #include "3rdparty/musashi/m68kcpu.h"
}
#include "3rdparty/z80/z80.h"
#include "3rdparty/ym/ym2610.h"

/*
    Few notes for later:

    - Some things are not 100% accurate: On a real machine when access to the SPR area is requested by writing to FF0121, the sprites disappear from screen.

    - When access to mapped memory is released by writing to FF0141, the area at E00000 is mostly FF but random bits of data are flashing.

    - SPR Bank select at FF01A1 seem to accept values 0-4. Any value above is the same as 4.
      I'm not entirely sure where 4 is supposed to point, that's theorically after the SPR ram.
*/

static const uint8_t bitReverseTable[] = {
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
    0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
    0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
    0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
    0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
    0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
    0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
    0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
    0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
    0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
    0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
    0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
    0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
    0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};

inline bool isCdAudioPlaying()
{
    return neocd.cdrom.isPlaying() && neocd.cdrom.isAudio();
}

inline const AudioBuffer::Sample& currentCdAudioSample()
{
    const int32_t currentSample = neocd.audio.buffer.masterCyclesThisFrameToSampleClamped(neocd.m68kMasterCyclesThisFrame());
    return neocd.audio.buffer.cdSamples[currentSample];
}

inline uint16_t reverseBits(uint16_t value)
{
    return (bitReverseTable[value & 0xFF] << 8) | (bitReverseTable[value >> 8]);
}

static uint32_t cdInterfaceReadByte(uint32_t address)
{
    switch (address)
    {
    case 0x0017:    // Unknown registers 
        break;

    case 0x0103:    // FF0103: CDROM Read Register
        return neocd.lc8951.readRegister();

    case 0x0161:    // FF0161: CDROM Communication: Read Response Packet
        return neocd.lc8951.readResponsePacket();

    case 0x0167:    // FF0167: Unknown. Front loader BIOS uses this
        break;

    default:
        LOG(LOG_INFO, "CD-UNIT: Byte read from unknown register %06X @ PC=%06X\n", address + 0xFF0000, m68k_get_reg(NULL, M68K_REG_PPC));
        break;
    }

    return 0x00;
}

static uint32_t cdInterfaceReadWord(uint32_t address)
{
    switch (address)
    {
    case 0x0004:    // FF0004: IRQ Mask for VBL & others
                    // /!\ It is really important that this register can be read as it is saved on the stack and restored during an interrupt.
        return neocd.irqMask2;

    case 0x011C:    /*
                        FF011C: System Config
                        00ST00NN 00000000
                        S = 1 to show an eject button in Top Loader BIOS
                        N = Nationality
                        T = Tray (Top & Front Models: Open=0 Close=1  CDZ: Open=1 Close=0)
                    */
        if (neocd.isCDZ())
            return ((~neocd.machineNationality & 7) << 8);
        return ((~neocd.machineNationality & 7) << 8) | 0x1000;

    case 0x0188:    // FF0188: Current CD-Audio Sample: Left Channel, bits are reversed
        if (isCdAudioPlaying())
            return reverseBits(currentCdAudioSample().left);

        return 0x0000;

    case 0x018A:    // FF018A: Current CD-Audio Sample: Right Channel, bits are reversed
        if (isCdAudioPlaying())
            return reverseBits(currentCdAudioSample().right);

        return 0x0000;

    default:
        LOG(LOG_INFO, "CD-UNIT: Word read from unknown register %06X @ PC=%06X\n", address + 0xFF0000, m68k_get_reg(NULL, M68K_REG_PPC));
        break;
    }

    return 0x0000;
}

static void cdInterfaceWriteByte(uint32_t address, uint32_t data)
{
    switch (address)
    {
    case 0x000D:    // Unknown Registers
    case 0x000E:
    case 0x0011:
    case 0x0015:
    case 0x0017:
    case 0x0167:
    case 0x016D:
//      LOG(LOG_INFO, "%06X = %02X\n", address + 0xFF0000, data);
        break;

    case 0x000F:    /*
                        FF000F: CDROM IRQ Acknowledge
                        $20 -> Vector $54 (CD-ROM decoder, IRQ1)
                        $10 -> Vector $58 (CD-ROM communication, IRQ2)
                        $08 -> Vector $5C (Unused)
                        $04 -> Vector $60 (Unused, Spurious Interrupt)
                    */
        if (data & 0x20)
            neocd.clearInterrupt(NeoGeoCD::CdromDecoder);

        if (data & 0x10)
            neocd.clearInterrupt(NeoGeoCD::CdromCommunication);

        neocd.updateInterrupts();
        break;

    case 0x0061:    // FF0061: DMA control $40 = enable / $00 = clear
        if (data == 0x40)
            neocd.memory.doDma();
        else if (data == 0)
            neocd.memory.resetDma();
        break;

    case 0x0101:    // FF0101: CDROM Register Select
        neocd.lc8951.setRegisterPointer(data);
        break;

    case 0x0103:    // FF0103: CDROM Write Register
        neocd.lc8951.writeRegister(data);
        break;

    case 0x0105:    // FF0105: Destination Area  (0=SPR; 1=PCM; 4=Z80; 5=FIX)
        switch (data)
        {
        case 0:
            neocd.memory.areaSelect = Memory::AREA_SPR;
            break;
        case 1:
            neocd.memory.areaSelect = Memory::AREA_PCM;
            break;
        case 4:
            neocd.memory.areaSelect = Memory::AREA_Z80;
            break;
        case 5:
            neocd.memory.areaSelect = Memory::AREA_FIX;
            break;
        default:
            neocd.memory.areaSelect = 0;
            break;
        }
        break;

    case 0x0111:    // FF0111: SPR Layer Enable / Disable
        neocd.video.sprDisable = (data != 0);
        break;

    case 0x0115:    // FF0115: FIX Layer Enable / Disable
        neocd.video.fixDisable = (data != 0);
        break;

    case 0x0119:    // FF0119: Video Enable / Disable
        neocd.video.videoEnable = (data != 0);
        break;

    case 0x0121:    // FF0121: SPR RAM Bus Request
        neocd.memory.busRequest |= Memory::AREA_SPR;
        break;

    case 0x0123:    // FF0123: PCM RAM Bus Request
        neocd.memory.busRequest |= Memory::AREA_PCM;
        break;

    case 0x0127:    // FF0127: Z80 RAM Bus Request
        neocd.memory.busRequest |= Memory::AREA_Z80;
        break;

    case 0x0129:    // FF0129: FIX RAM Bus Request
        neocd.memory.busRequest |= Memory::AREA_FIX;
        break;

    case 0x0141:    // FF0141: SPR RAM Bus Release
        neocd.memory.busRequest &= ~Memory::AREA_SPR;
        break;

    case 0x0143:    // FF0143: PCM RAM Bus Release
        neocd.memory.busRequest &= ~Memory::AREA_PCM;
        break;

    case 0x0147:    // FF0147: Z80 RAM Bus Release
        neocd.memory.busRequest &= ~Memory::AREA_Z80;
        break;

    case 0x0149:    // FF0149: FIX RAM Bus Release
        neocd.memory.busRequest &= ~Memory::AREA_FIX;
        neocd.video.updateFixUsageMap();
        break;

    case 0x0163:    // FF0163: CDROM Communication, Send Command Packet
        neocd.lc8951.writeCommandPacket(data);
        break;

    case 0x0165:    // FF0165: CDROM Communication, Access Pointer Increment + "Data Clock" */
        neocd.lc8951.increasePacketPointer(data);
        break;

    case 0x016F:    // FF016F: Watchdog Timer $00 Enable / $01 Disable
        if (data)
            neocd.timers.timer<TimerGroup::Watchdog>().setState(Timer::Stopped);
        else
            neocd.timers.timer<TimerGroup::Watchdog>().setState(Timer::Active);
        break;

    case 0x0181:    /*
                        FF0181: CD Communication Reset (Active Low)
                        When data = 0, CD communication is disabled (and no IRQ will trigger)
                        When data = 1, CD communication is enabled
                        
                        This is evidenced by the fact that if a communication timeout is detected (flag $76DB(A5))
                        this register will be set to zero during the next VBL then back to 1 four frames later.

                        This register has no influence whatsoever on the decoder IRQ (Verified on real hardware)
                    */
//      LOG(LOG_INFO, "FF0181: %02X\n", data);
        neocd.cdCommunicationNReset = (data != 0);
        neocd.lc8951.resetPacketPointers();
        break;

    case 0x0183:    // FF0183: Z80 $00 Reset / $FF Enable
        if (!data)
            neocd.z80Disable = true;
        else
        {
            neocd.z80Disable = false;
            z80_reset();
            YM2610Reset();
        }
        break;

    case 0x01A1:    // FF01A1: SPR RAM Bank Select
        neocd.memory.sprBankSelect = data;
        break;

    case 0x01A3:    // FF01A3: PCM RAM Bank Select
        neocd.memory.pcmBankSelect = data;
        break;

    default:
        LOG(LOG_INFO, "CD-UNIT: Write to unknown register %06X @ PC=%06X DATA=%02X\n", address + 0xFF0000, m68k_get_reg(NULL, M68K_REG_PPC), data);
        break;
    }
}

static void cdInterfaceWriteWord(uint32_t address, uint32_t data)
{
    switch (address)
    {
    // Unknown Registers
    case 0x0006:
    case 0x0008:
    case 0x000A:
//      LOG(LOG_INFO, "%06X = %04X\n", address + 0xFF0000, data);
        break;

    case 0x0000:    /*
                        FF0000: CD-ROM Reset. (To be verified)
                    */
//      LOG(LOG_INFO, "CDROM: Drive reset at PC=%06X\n", m68k_get_reg(NULL, M68K_REG_PPC));
        neocd.cdrom.stop();
        neocd.lc8951.controller.status = CdromController::CdIdle;
        break;

    case 0x0002:    /*
                        FF0002: CDROM Interrupt Mask
                        0x550
                        0x050 IRQ2
                        0x500 IRQ1
                    */
        neocd.irqMask1 = data;
//      LOG(LOG_INFO, "IRQ MASK1=%04X MASK2=%04X\n", neocd.irqMask1, neocd.irqMask2);

        // Used to detect disc activity in the main loop
        if (neocd.isCdDecoderIRQEnabled())
            neocd.irq1EnabledThisFrame = true;
        break;

    case 0x0004:    /*
                        FF0004: VBL Interrupt Mask
                        0x731
                        0x700   HBL
                        0x030   VBL
                        0x001   Unknown, writing zero causes a hard reset.
                    
                        /!\ Emulating this register is vital !
                        When the system is loading data from CD-ROM the VBL IRQ is disabled using this mask.
                        The VBL IRQ reconfigure mapped memory access to peek into the Z80 RAM, if this register is not emulated the system will end up overwriting the Z80 RAM.
                    */
        neocd.irqMask2 = data;
//      LOG(LOG_INFO, "IRQ MASK1=%04X MASK2=%04X\n", neocd.irqMask1, neocd.irqMask2);
        break;

    case 0x0064:    // FF0064: DMA Destination Address High Word
        neocd.memory.dmaDestination = (neocd.memory.dmaDestination & 0xFFFF) | ((uint32_t)data << 16);
        break;

    case 0x0066:    // FF0066: DMA Destination Address Low Word
        neocd.memory.dmaDestination = (neocd.memory.dmaDestination & 0xFFFF0000) | (uint32_t)data;
        break;

    case 0x0068:    // FF0068: DMA Source Address High Word
        neocd.memory.dmaSource = (neocd.memory.dmaSource & 0xFFFF) | ((uint32_t)data << 16);
        break;

    case 0x006A:    // FF006A: DMA Source Address Low Word
        neocd.memory.dmaSource = (neocd.memory.dmaSource & 0xFFFF0000) | (uint32_t)data;
        break;

    case 0x006C:    // FF006C: DMA Pattern
        neocd.memory.dmaPattern = data;
        break;

    case 0x0070:    // FF0070: DMA Length High Word
        neocd.memory.dmaLength = (neocd.memory.dmaLength & 0xFFFF) | ((uint32_t)data << 16);
        break;

    case 0x0072:    // FF0072: DMA Length Low Word
        neocd.memory.dmaLength = (neocd.memory.dmaLength & 0xFFFF0000) | (uint32_t)data;
        break;

    case 0x007E:    // FF007E: DMA Configuration Register 0
        neocd.memory.dmaConfig[0] = data;
        break;

    case 0x0080:    // FF0080: DMA Configuration Register 1
        neocd.memory.dmaConfig[1] = data;
        break;

    case 0x0082:    // FF0082: DMA Configuration Register 2
        neocd.memory.dmaConfig[2] = data;
        break;

    case 0x0084:    // FF0084: DMA Configuration Register 3
        neocd.memory.dmaConfig[3] = data;
        break;

    case 0x0086:    // FF0086: DMA Configuration Register 4
        neocd.memory.dmaConfig[4] = data;
        break;

    case 0x0088:    // FF0088: DMA Configuration Register 5
        neocd.memory.dmaConfig[5] = data;
        break;

    case 0x008A:    // FF008A: DMA Configuration Register 6
        neocd.memory.dmaConfig[6] = data;
        break;

    case 0x008C:    // FF008C: DMA Configuration Register 7
        neocd.memory.dmaConfig[7] = data;
        break;

    case 0x008E:    // FF008E: DMA Configuration Register 8
        neocd.memory.dmaConfig[8] = data;
        break;

    default:
        LOG(LOG_INFO, "CD-UNIT: Write to unknown register %06X @ PC=%06X DATA=%04X\n", address + 0xFF0000, m68k_get_reg(NULL, M68K_REG_PPC), data);
        break;
    }
}

const Memory::Handlers cdInterfaceHandlers = {
    cdInterfaceReadByte,
    cdInterfaceReadWord,
    cdInterfaceWriteByte,
    cdInterfaceWriteWord
};

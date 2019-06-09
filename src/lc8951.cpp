#include "lc8951.h"
#include "neogeocd.h"
extern "C" {
    #include "3rdparty/musashi/m68kcpu.h"
}

#include <algorithm>
#include <cstring>

LC8951::LC8951() :
    status(CdIdle),
    registerPointer(0),
    commandPointer(0),
    responsePointer(0),
    strobe(0),
    SBOUT(0),
    IFCTRL(0),
    DBCL(0),
    DBCH(0),
    DACL(0),
    DACH(0),
    DTRG(0),
    DTACK(0),
    WAL(0),
    WAH(0),
    CTRL0(0),
    CTRL1(0),
    PTL(0),
    PTH(0),
    COMIN(0),
    IFSTAT(0),
    HEAD0(0),
    HEAD1(0),
    HEAD2(0),
    HEAD3(0),
    STAT0(0),
    STAT1(0),
    STAT2(0),
    STAT3(0)
{
    reset();
}

void LC8951::reset()
{
    status = CdIdle;

    std::memset(commandPacket, 0, sizeof(commandPacket));
    std::memset(responsePacket, 0, sizeof(responsePacket));
    setPacketChecksum(responsePacket);

    registerPointer = 0;
    resetPacketPointers();

    updateHeadRegisters(0);
    
    SBOUT = 0;
    IFCTRL = 0;
    DBCL = 0;
    DBCH = 0;
    DACL = 0;
    DACH = 0;
    DTRG = 0;
    DTACK = 0;
    WAL = 0x30; // 2352
    WAH = 0x09;
    CTRL0 = 0;
    CTRL1 = 0;
    PTL = 0;
    PTH = 0;
    COMIN = 0;
    IFSTAT = 0xFF;
    STAT0 = 0;
    STAT1 = 0;
    STAT2 = 0;
    STAT3 = 0;
    
    std::memset(buffer, 0, sizeof(buffer));
}

void LC8951::updateHeadRegisters(uint32_t lba)
{
    uint32_t m, s, f;
    
    CDROM_LOG(LOG_INFO, "CD LBA = %d\n", lba);

    CdromToc::toMSF(CdromToc::fromLBA(lba), m, s, f);

    HEAD0 = Cdrom::toBCD(m);
    HEAD1 = Cdrom::toBCD(s);
    HEAD2 = Cdrom::toBCD(f);
    HEAD3 = 0x01;
}

uint8_t LC8951::readRegister()
{
    uint8_t value;
    
    switch (registerPointer & 0xF)
    {
    case 0x00:  // COMIN
        value = COMIN;
        CDROM_LOG(LOG_INFO, "READ COMIN = %02X\n", value);
        break;
    case 0x01:  // IFSTAT
        value = IFSTAT;
        CDROM_LOG(LOG_INFO, "READ IFSTAT = %02X\n", value);
        break;
    case 0x02:  // DBCL
        value = DBCL;
        CDROM_LOG(LOG_INFO, "READ DBCL = %02X\n", value);
        break;
    case 0x03:  // DCBH
        value = DBCH;
        CDROM_LOG(LOG_INFO, "READ DBCH = %02X\n", value);
        break;
    case 0x04:  // HEAD0
        value = (CTRL1 & SHDREN) ? 0 : HEAD0;
        CDROM_LOG(LOG_INFO, "READ HEAD0 = %02X\n", value);
        break;
    case 0x05:  // HEAD1
        value = (CTRL1 & SHDREN) ? 0 : HEAD1;
        CDROM_LOG(LOG_INFO, "READ HEAD1 = %02X\n", value);
        break;
    case 0x06:  // HEAD2
        value = (CTRL1 & SHDREN) ? 0 : HEAD2;
        CDROM_LOG(LOG_INFO, "READ HEAD2 = %02X\n", value);
        break;
    case 0x07:  // HEAD3
        value = (CTRL1 & SHDREN) ? 0 : HEAD3;
        CDROM_LOG(LOG_INFO, "READ HEAD3 = %02X\n", value);
        break;
    case 0x08: // PTL
        value = PTL;
        CDROM_LOG(LOG_INFO, "READ PTL = %02X\n", value);
        break;
    case 0x09: // PTH
        value = PTH;
        CDROM_LOG(LOG_INFO, "READ PTH = %02X\n", value);
        break;
    case 0x0A:  // WAL
        value = WAL;
        CDROM_LOG(LOG_INFO, "READ WAL = %02X\n", value);
        break;
    case 0x0B:  // WAH
        value = WAH;
        CDROM_LOG(LOG_INFO, "READ WAH = %02X\n", value);
        break;
    case 0x0C:  // STAT0
        value = STAT0;
        CDROM_LOG(LOG_INFO, "READ STAT0 = %02X\n", value);
        break;
    case 0x0D:  // STAT1
        value = STAT1;
        CDROM_LOG(LOG_INFO, "READ STAT1 = %02X\n", value);
        break;
    case 0x0E:  // STAT2
        value = STAT2;
        CDROM_LOG(LOG_INFO, "READ STAT2 = %02X\n", value);
        break;
    case 0x0F:  // STAT3
        value = STAT3;
        CDROM_LOG(LOG_INFO, "READ STAT3 = %02X\n", value);
        
        // Reading STAT3 clears DECI
        IFSTAT = IFSTAT & ~DECI;
        break;
    }

    incrementRegisterPointer();
    return value;
}

void LC8951::writeRegister(uint8_t data)
{
    switch (registerPointer & 0xF)
    {
    case 0x00:
        CDROM_LOG(LOG_INFO, "WRITE SBOUT = %02X\n", data);
        SBOUT = data;
        break;
    case 0x01:
        CDROM_LOG(LOG_INFO, "WRITE IFCTRL = %02X\n", data);
        IFCTRL = data;
        break;
    case 0x02:
        CDROM_LOG(LOG_INFO, "WRITE DBCL = %02X\n", data);
        DBCL = data;
        break;
    case 0x03:
        CDROM_LOG(LOG_INFO, "WRITE DBCH = %02X\n", data);
        DBCH = data;
        break;
    case 0x04:
        CDROM_LOG(LOG_INFO, "WRITE DACL = %02X\n", data);
        DACL = data;
        break;
    case 0x05:
        CDROM_LOG(LOG_INFO, "WRITE DACH = %02X\n", data);
        DACH = data;
        break;
    case 0x06:
        CDROM_LOG(LOG_INFO, "WRITE DTRG = %02X\n", data);
        DTRG = data;
        
        // Writing DTRG sets !DTBSY if data output is enabled
        if (IFCTRL & DOUTEN)
            IFSTAT &= ~DTBSY;
        break;
    case 0x07:
        CDROM_LOG(LOG_INFO, "WRITE DTACK = %02X\n", data);
        DTACK = data;
        
        // Writing DTACK clears !DTEI
        IFSTAT = IFSTAT | DTEI;
        break;
    case 0x08:
        CDROM_LOG(LOG_INFO, "WRITE WAL = %02X\n", data);
        WAL = data;
        break;
    case 0x09:
        CDROM_LOG(LOG_INFO, "WRITE WAH = %02X\n", data);
        WAH = data;
        break;
    case 0x0A: // CTRL0
        CDROM_LOG(LOG_INFO, "WRITE CTRL0 = %02X\n", data);
        CTRL0 = data;
        break;
    case 0x0B:
        CDROM_LOG(LOG_INFO, "WRITE CTRL1 = %02X\n", data);
        CTRL1 = data;
        break;
    case 0x0C:
        CDROM_LOG(LOG_INFO, "WRITE PTL = %02X\n", data);
        PTL = data;
        break;
    case 0x0D:
        CDROM_LOG(LOG_INFO, "WRITE PTH = %02X\n", data);
        PTH = data;
        break;
    case 0x0E:  // CTRL2
        CDROM_LOG(LOG_INFO, "WRITE CTRL2 = %02X\n", data);
        break;
    case 0x0F:  // RESET
        CDROM_LOG(LOG_INFO, "LC8951 RESET = %02X\n", data);
        break;
    }

    incrementRegisterPointer();
}

void LC8951::incrementRegisterPointer()
{
    if (!registerPointer)
        return;

    registerPointer = (registerPointer & 0xF0) | ((registerPointer + 1) & 0x0F);
}

uint8_t LC8951::readResponsePacket()
{
    if (responsePointer & 1)
        return ((responsePacket[responsePointer >> 1] & 0x0F) | (strobe << 4));

    return ((responsePacket[responsePointer >> 1] >> 4) | (strobe << 4));
}

void LC8951::writeCommandPacket(uint8_t data)
{
    if (commandPointer & 1)
        commandPacket[commandPointer >> 1] = (commandPacket[commandPointer >> 1] & 0xF0) | (data & 0x0F);
    else
        commandPacket[commandPointer >> 1] = (commandPacket[commandPointer >> 1] & 0x0F) | (data << 4);
}

void LC8951::increasePacketPointer(uint8_t data)
{
    switch (data)
    {
    case 0x00:
        break;
    case 0x01:
        commandPointer = (commandPointer + 1) % 10;
        if (!commandPointer)
            processCdCommand();
        break;
    case 0x02:
        strobe = 0;
        responsePointer = (responsePointer + 1) % 10;
        break;
    case 0x03:
        strobe = 1;
        break;
    }
}

void LC8951::resetPacketPointers()
{
    commandPointer = 0;
    responsePointer = 9;
    strobe = 1;
}

void LC8951::setRegisterPointer(uint8_t value)
{
    registerPointer = value;
}

uint8_t LC8951::calculatePacketChecksum(const uint8_t* packet)
{
    uint8_t checksum = 0;

    for (int i = 0; i < 4; ++i)
    {
        checksum += (packet[i] >> 4);
        checksum += (packet[i] & 0x0F);
    }

    checksum += (packet[4] >> 4);
    checksum += 5;
    checksum = (~checksum) & 0x0F;

    return checksum;
}

void LC8951::setPacketChecksum(uint8_t* packet)
{
    uint8_t checksum = calculatePacketChecksum(packet);
    packet[4] = (packet[4] & 0xF0) | checksum;
}

// Not really done by the LC8951 but by the CD-ROM board behind it
void LC8951::processCdCommand()
{
    if (commandPacket[0])
    {
        CDROM_LOG(LOG_INFO, "COMMAND %02X%02X%02X%02X%02X\n",
            commandPacket[0],
            commandPacket[1],
            commandPacket[2],
            commandPacket[3],
            commandPacket[4]);
    }

    if ((commandPacket[4] & 0x0F) != calculatePacketChecksum(commandPacket))
    {
        CDROM_LOG(LOG_INFO, "CD command with wrong checksum!");
        responsePacket[0] = status;
        responsePacket[1] = 0x00;
        responsePacket[2] = 0x00;
        responsePacket[3] = 0x00;
        responsePacket[4] = 0x00;
        setPacketChecksum(responsePacket);
        return;
    }

    switch (commandPacket[0])
    {
    case 0x00:  // Status
        responsePacket[0] = (responsePacket[0] & 0x0F) | status;
        break;

    case 0x10:  // Stop
        neocd.cdrom.stop();
        status = CdIdle;
        responsePacket[0] = status;
        responsePacket[1] = 0x00;
        responsePacket[2] = 0x00;
        responsePacket[3] = 0x00;
        responsePacket[4] = 0x00;
        break;

    case 0x20:  // Query info
        if ((status == CdIdle) && (!neocd.cdrom.isTocEmpty()))
            status = CdStopped;

        switch (commandPacket[1] & 0x0F)    // Sub command
        {
        case 0x00:  // Get current position
        {
            uint32_t     m, s, f;
            CdromToc::toMSF(CdromToc::fromLBA(neocd.cdrom.position()), m, s, f);
            responsePacket[0] = status;
            responsePacket[1] = Cdrom::toBCD(m);
            responsePacket[2] = Cdrom::toBCD(s);
            responsePacket[3] = Cdrom::toBCD(f);
            responsePacket[4] = neocd.cdrom.isData() ? 0x40 : 0x00;
        }
        break;

        case 0x01:  // Get current position (relative)
        {
            uint32_t position;

            if (neocd.cdrom.isPregap()) // Pregaps are counted down
                position = (neocd.cdrom.currentTrackPosition() + neocd.cdrom.currentIndexSize()) - (neocd.cdrom.position() + 1);
            else
                position = neocd.cdrom.position() - neocd.cdrom.currentTrackPosition();

            uint32_t m, s, f;
            CdromToc::toMSF(position, m, s, f);

            responsePacket[0] = status | 0x01;
            responsePacket[1] = Cdrom::toBCD(m);
            responsePacket[2] = Cdrom::toBCD(s);
            responsePacket[3] = Cdrom::toBCD(f);
            responsePacket[4] = neocd.cdrom.isData() ? 0x40 : 0x00;
        }
        break;

        case 0x02:  // Get current track
        {
            /*
                NOTE: Copy protection can be simulated by generating indexes for track 01 as follow:
                index = std::min(99, (((m * 100) + s) / 4) + 1);
                */
            TrackIndex trackIndex = neocd.cdrom.currentTrackIndex();
            responsePacket[0] = status | 0x02;
            responsePacket[1] = Cdrom::toBCD(trackIndex.track());
            responsePacket[2] = Cdrom::toBCD(trackIndex.index());
            responsePacket[3] = 0x00;
            responsePacket[4] = neocd.cdrom.isData() ? 0x40 : 0x00;
        }
        break;

        case 0x03: // Get leadout address (msf)
        {
            uint32_t m, s, f;
            CdromToc::toMSF(CdromToc::fromLBA(neocd.cdrom.leadout()), m, s, f);
            responsePacket[0] = status | 0x03;
            responsePacket[1] = Cdrom::toBCD(m);
            responsePacket[2] = Cdrom::toBCD(s);
            responsePacket[3] = Cdrom::toBCD(f);
            responsePacket[4] = 0x00;
        }
        break;

        case 0x04: // Get first track and last track
        {
            responsePacket[0] = status | 0x04;
            responsePacket[1] = Cdrom::toBCD(neocd.cdrom.firstTrack());
            responsePacket[2] = Cdrom::toBCD(neocd.cdrom.lastTrack());
            responsePacket[3] = 0x00;
            responsePacket[4] = 0x00;
        }
        break;

        case 0x05: // Get track info
        {
            uint8_t track = Cdrom::fromBCD(commandPacket[2]);
            uint32_t position = CdromToc::fromLBA(neocd.cdrom.trackPosition(track));
            uint32_t m, s, f;
            CdromToc::toMSF(position, m, s, f);

            responsePacket[0] = status | 0x05;
            responsePacket[1] = Cdrom::toBCD(m);
            responsePacket[2] = Cdrom::toBCD(s);

            if (neocd.cdrom.trackIsData(track))
                responsePacket[3] = Cdrom::toBCD(f) | 0x80;
            else
                responsePacket[3] = Cdrom::toBCD(f);

            responsePacket[4] = commandPacket[2] << 4;
        }
        break;

        case 0x06: // Check end of disc?
        {
            if (neocd.cdrom.position() >= neocd.cdrom.leadout())
                status = CdEndOfDisc;

            responsePacket[0] = status | 0x06;
            responsePacket[1] = 0x00;
            responsePacket[2] = 0x00;
            responsePacket[3] = 0x00;
            responsePacket[4] = neocd.cdrom.isData() ? 0x40 : 0x00;
        }
        break;

        case 0x07: // Unknown, CDZ copy protection related. (Disc recognition)
        {
            responsePacket[0] = status | 0x07;
            responsePacket[1] = 0x02; // Possible values: 2, 5, E, F. Most games want 2, Twinkle Star Sprites want F
            responsePacket[2] = 0x00;
            responsePacket[3] = 0x00;
            responsePacket[4] = 0x00;
        }
        break;

        default:
            CDROM_LOG(LOG_INFO, "CDROM: Received unknown 0x20 sub-command (%02X%02X%02X%02X%02X)\n",
                commandPacket[0],
                commandPacket[1],
                commandPacket[2],
                commandPacket[3],
                commandPacket[4]);
            responsePacket[0] = status;
            responsePacket[1] = 0x00;
            responsePacket[2] = 0x00;
            responsePacket[3] = 0x00;
            responsePacket[4] = 0x00;
            break;
        }
        break;

    case 0x30: // Play
    {
        uint8_t m, s, f;

        m = Cdrom::fromBCD(commandPacket[1]);
        s = Cdrom::fromBCD(commandPacket[2]);
        f = Cdrom::fromBCD(commandPacket[3]);

        uint32_t position = CdromToc::toLBA(CdromToc::fromMSF(m, s, f));

        /*
            CDZ Bios sometimes send the play command twice.
            There is a command FIFO at $7472(A5)
            Read pointer is $7650(A5) and write pointer $7651(A5)
            The code at $C0B4B2 writes 7 (Send play command) to the FIFO and is systematically called twice.
            Some tracks use this, other tracks use $C0B3C6 (called once)
            Is this a bug in the BIOS? Or is something not properly emulated?
        */
        neocd.cdrom.play();
        neocd.cdrom.seek(position);
//        updateHeadRegisters(neocd.cdrom.position());

        status = CdPlaying;
        responsePacket[0] = status | 0x02;
        responsePacket[1] = Cdrom::toBCD(neocd.cdrom.currentTrackIndex().track());
        responsePacket[2] = 0x00;
        responsePacket[3] = 0x00;
        responsePacket[4] = 0x00;
    }
        break;

    case 0x40: // Seek
        CDROM_LOG(LOG_INFO, "CDROM: Received seek command, ignored (%02X%02X%02X%02X%02X)\n",
            commandPacket[0],
            commandPacket[1],
            commandPacket[2],
            commandPacket[3],
            commandPacket[4]);

        neocd.cdrom.stop();
        status = CdPaused;
        responsePacket[0] = CdSeeking;
        responsePacket[1] = 0x00;
        responsePacket[2] = 0x00;
        responsePacket[3] = 0x00;
        responsePacket[4] = 0x00;
        break;

    case 0x50: // Unknown, CDZ only
        responsePacket[0] = status;
/*        responsePacket[1] = 0x00;
        responsePacket[2] = 0x00;
        responsePacket[3] = 0x00;
        responsePacket[4] = 0x00;*/
        break;

    case 0x60: // Pause
        neocd.cdrom.stop();

        status = CdPaused;
        responsePacket[0] = status;
/*        responsePacket[1] = 0x00;
        responsePacket[2] = 0x00;
        responsePacket[3] = 0x00;
        responsePacket[4] = 0x00;*/
        break;

    case 0x70: // Resume
        neocd.cdrom.play();

        status = CdPlaying;
        responsePacket[0] = status;
/*        responsePacket[1] = 0x00;
        responsePacket[2] = 0x00;
        responsePacket[3] = 0x00;
        responsePacket[4] = 0x00;*/
        break;

    case 0x80: // Scan forward
    {
        uint32_t position = neocd.cdrom.position();

        position = std::min(position + SCAN_SPEED, neocd.cdrom.leadout() - 1);

        neocd.cdrom.seek(position);
//        updateHeadRegisters(neocd.cdrom.position());

        status = CdPlaying;
        responsePacket[0] = CdScanning;
/*        responsePacket[1] = 0x00;
        responsePacket[2] = 0x00;
        responsePacket[3] = 0x00;
        responsePacket[4] = 0x00;*/
    }
        break;

    case 0x90: // Scan backward
    {
        uint32_t position = neocd.cdrom.position();

        if (position < SCAN_SPEED)
            position = 0;
        else
            position -= SCAN_SPEED;

        neocd.cdrom.seek(position);
//        updateHeadRegisters(neocd.cdrom.position());

        status = CdPlaying;
        responsePacket[0] = CdScanning;
/*        responsePacket[1] = 0x00;
        responsePacket[2] = 0x00;
        responsePacket[3] = 0x00;
        responsePacket[4] = 0x00;*/
    }
        break;

    case 0xB0: // Move to track
    {
        uint8_t track = Cdrom::fromBCD(commandPacket[1]);
        uint32_t position = neocd.cdrom.trackPosition(track);

        neocd.cdrom.play();
        neocd.cdrom.seek(position);
//        updateHeadRegisters(neocd.cdrom.position());

        status = CdPlaying;
        responsePacket[0] = status | 0x02;
        responsePacket[1] = Cdrom::toBCD(neocd.cdrom.currentTrackIndex().track());
        responsePacket[2] = 0x00;
        responsePacket[3] = 0x00;
        responsePacket[4] = 0x00;
    }
        break;

    case 0x02: // More CD Protection Stuff?
    case 0x13: // The results of those are written to $778C(A5)
    case 0x23: // Then transferred to $800076 which is in the backup RAM... What for?
    case 0x33:
    case 0x43:
    case 0x53:
    case 0x63:
    case 0xE2:
        responsePacket[0] = status;
        responsePacket[1] = 0x00;
        responsePacket[2] = 0x00;
        responsePacket[3] = 0x00;
        responsePacket[4] = 0x00;
        break;

    default:
/*      CDROM_LOG(LOG_INFO, "COMMAND %02X%02X%02X%02X%02X\n",
            commandPacket[0],
            commandPacket[1],
            commandPacket[2],
            commandPacket[3],
            commandPacket[4]);*/
        responsePacket[0] = status;
        responsePacket[1] = 0x00;
        responsePacket[2] = 0x00;
        responsePacket[3] = 0x00;
        responsePacket[4] = 0x00;
        break;
    }

    setPacketChecksum(responsePacket);

/*  CDROM_LOG(LOG_INFO, "ANSWER  %02X%02X%02X%02X%02X\n",
        responsePacket[0],
        responsePacket[1],
        responsePacket[2],
        responsePacket[3],
        responsePacket[4]);*/
}

void LC8951::sectorDecoded()
{
    // If the decoder is not enabled, nothing to do
    if (!(CTRL0 & DECEN))
        return;
        
    // Update the head registers
    updateHeadRegisters(neocd.cdrom.position());
    
    // If the current track is not a data track, nothing more to do
    if (!neocd.cdrom.isData())
        return;
    
    // Read the sector in buffer
    // The Neo Geo CD never change the write address (WA) or pointer registers (PT)
    // It simply read PT and set DAC to PT + 4 (to skip the header) and DBC to 0x7FF
    // This means we only need keep the last decoded sector
    neocd.cdrom.readData(reinterpret_cast<char*>(buffer));

    // Autoincrement WA and PT
    addWordRegister(WAL, WAH, 2352);
    addWordRegister(PTL, PTH, 2352);

    STAT0 = CRCOK;
    STAT1 = 0;
    
    if (CTRL0 & AUTORQ)
        STAT2 = CTRL1 & MODRQ;
    else
        STAT2 = CTRL1 & (MODRQ | FORMRQ);
    
    STAT3 = 0;
    
    // Set !DECI
    IFSTAT &= ~DECI;
}

void LC8951::endTransfer()
{
    IFSTAT |= DTBSY;
    
    addWordRegister(DACL, DACH, wordRegister(DBCL, DBCH) + 1);
    setWordRegister(DBCL, DBCH, 0);
}

DataPacker& operator<<(DataPacker& out, const LC8951& lc8951)
{
    out << lc8951.status;
    out << lc8951.registerPointer;
    out << lc8951.commandPacket;
    out << lc8951.commandPointer;
    out << lc8951.responsePacket;
    out << lc8951.responsePointer;
    out << lc8951.strobe;
    out << lc8951.SBOUT;
    out << lc8951.IFCTRL;
    out << lc8951.DBCL;
    out << lc8951.DBCH;
    out << lc8951.DACL;
    out << lc8951.DACH;
    out << lc8951.DTRG;
    out << lc8951.DTACK;
    out << lc8951.WAL;
    out << lc8951.WAH;
    out << lc8951.CTRL0;
    out << lc8951.CTRL1;
    out << lc8951.PTL;
    out << lc8951.PTH;
    out << lc8951.COMIN;
    out << lc8951.IFSTAT;
    out << lc8951.HEAD0;
    out << lc8951.HEAD1;
    out << lc8951.HEAD2;
    out << lc8951.HEAD3;
    out << lc8951.STAT0;
    out << lc8951.STAT1;
    out << lc8951.STAT2;
    out << lc8951.STAT3;
    out << lc8951.buffer;
    
    return out;
}

DataPacker& operator>>(DataPacker& in, LC8951& lc8951)
{
    in >> lc8951.status;
    in >> lc8951.registerPointer;
    in >> lc8951.commandPacket;
    in >> lc8951.commandPointer;
    in >> lc8951.responsePacket;
    in >> lc8951.responsePointer;
    in >> lc8951.strobe;
    in >> lc8951.SBOUT;
    in >> lc8951.IFCTRL;
    in >> lc8951.DBCL;
    in >> lc8951.DBCH;
    in >> lc8951.DACL;
    in >> lc8951.DACH;
    in >> lc8951.DTRG;
    in >> lc8951.DTACK;
    in >> lc8951.WAL;
    in >> lc8951.WAH;
    in >> lc8951.CTRL0;
    in >> lc8951.CTRL1;
    in >> lc8951.PTL;
    in >> lc8951.PTH;
    in >> lc8951.COMIN;
    in >> lc8951.IFSTAT;
    in >> lc8951.HEAD0;
    in >> lc8951.HEAD1;
    in >> lc8951.HEAD2;
    in >> lc8951.HEAD3;
    in >> lc8951.STAT0;
    in >> lc8951.STAT1;
    in >> lc8951.STAT2;
    in >> lc8951.STAT3;
    in >> lc8951.buffer;
    
    return in;
}

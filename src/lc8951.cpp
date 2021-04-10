#include <algorithm>
#include <cstring>

#include "lc8951.h"
#include "neogeocd.h"
extern "C" {
    #include "3rdparty/musashi/m68kcpu.h"
}

//#define LC8951_LOG(x, ...) LOG(x, __VA_ARGS__)
#define LC8951_LOG(x, ...)

LC8951::LC8951() :
    controller(),
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
    controller.reset();

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
    
    LC8951_LOG(LOG_INFO, "CD LBA = %d\n", lba);

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
        LC8951_LOG(LOG_INFO, "READ COMIN = %02X\n", value);
        break;
    case 0x01:  // IFSTAT
        value = IFSTAT;
        LC8951_LOG(LOG_INFO, "READ IFSTAT = %02X\n", value);
        break;
    case 0x02:  // DBCL
        value = DBCL;
        LC8951_LOG(LOG_INFO, "READ DBCL = %02X\n", value);
        break;
    case 0x03:  // DCBH
        value = DBCH;
        LC8951_LOG(LOG_INFO, "READ DBCH = %02X\n", value);
        break;
    case 0x04:  // HEAD0
        value = (CTRL1 & SHDREN) ? 0 : HEAD0;
        LC8951_LOG(LOG_INFO, "READ HEAD0 = %02X\n", value);
        break;
    case 0x05:  // HEAD1
        value = (CTRL1 & SHDREN) ? 0 : HEAD1;
        LC8951_LOG(LOG_INFO, "READ HEAD1 = %02X\n", value);
        break;
    case 0x06:  // HEAD2
        value = (CTRL1 & SHDREN) ? 0 : HEAD2;
        LC8951_LOG(LOG_INFO, "READ HEAD2 = %02X\n", value);
        break;
    case 0x07:  // HEAD3
        value = (CTRL1 & SHDREN) ? 0 : HEAD3;
        LC8951_LOG(LOG_INFO, "READ HEAD3 = %02X\n", value);
        break;
    case 0x08: // PTL
        value = PTL;
        LC8951_LOG(LOG_INFO, "READ PTL = %02X\n", value);
        break;
    case 0x09: // PTH
        value = PTH;
        LC8951_LOG(LOG_INFO, "READ PTH = %02X\n", value);
        break;
    case 0x0A:  // WAL
        value = WAL;
        LC8951_LOG(LOG_INFO, "READ WAL = %02X\n", value);
        break;
    case 0x0B:  // WAH
        value = WAH;
        LC8951_LOG(LOG_INFO, "READ WAH = %02X\n", value);
        break;
    case 0x0C:  // STAT0
        value = STAT0;
        LC8951_LOG(LOG_INFO, "READ STAT0 = %02X\n", value);
        break;
    case 0x0D:  // STAT1
        value = STAT1;
        LC8951_LOG(LOG_INFO, "READ STAT1 = %02X\n", value);
        break;
    case 0x0E:  // STAT2
        value = STAT2;
        LC8951_LOG(LOG_INFO, "READ STAT2 = %02X\n", value);
        break;
    case 0x0F:  // STAT3
        value = STAT3;
        LC8951_LOG(LOG_INFO, "READ STAT3 = %02X\n", value);
        
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
        LC8951_LOG(LOG_INFO, "WRITE SBOUT = %02X\n", data);
        SBOUT = data;
        break;
    case 0x01:
        LC8951_LOG(LOG_INFO, "WRITE IFCTRL = %02X\n", data);
        IFCTRL = data;
        break;
    case 0x02:
        LC8951_LOG(LOG_INFO, "WRITE DBCL = %02X\n", data);
        DBCL = data;
        break;
    case 0x03:
        LC8951_LOG(LOG_INFO, "WRITE DBCH = %02X\n", data);
        DBCH = data;
        break;
    case 0x04:
        LC8951_LOG(LOG_INFO, "WRITE DACL = %02X\n", data);
        DACL = data;
        break;
    case 0x05:
        LC8951_LOG(LOG_INFO, "WRITE DACH = %02X\n", data);
        DACH = data;
        break;
    case 0x06:
        LC8951_LOG(LOG_INFO, "WRITE DTRG = %02X\n", data);
        DTRG = data;
        
        // Writing DTRG sets !DTBSY if data output is enabled
        if (IFCTRL & DOUTEN)
            IFSTAT &= ~DTBSY;
        break;
    case 0x07:
        LC8951_LOG(LOG_INFO, "WRITE DTACK = %02X\n", data);
        DTACK = data;
        
        // Writing DTACK clears !DTEI
        IFSTAT = IFSTAT | DTEI;
        break;
    case 0x08:
        LC8951_LOG(LOG_INFO, "WRITE WAL = %02X\n", data);
        WAL = data;
        break;
    case 0x09:
        LC8951_LOG(LOG_INFO, "WRITE WAH = %02X\n", data);
        WAH = data;
        break;
    case 0x0A: // CTRL0
        LC8951_LOG(LOG_INFO, "WRITE CTRL0 = %02X\n", data);
        CTRL0 = data;
        break;
    case 0x0B:
        LC8951_LOG(LOG_INFO, "WRITE CTRL1 = %02X\n", data);
        CTRL1 = data;
        break;
    case 0x0C:
        LC8951_LOG(LOG_INFO, "WRITE PTL = %02X\n", data);
        PTL = data;
        break;
    case 0x0D:
        LC8951_LOG(LOG_INFO, "WRITE PTH = %02X\n", data);
        PTH = data;
        break;
    case 0x0E:  // CTRL2
        LC8951_LOG(LOG_INFO, "WRITE CTRL2 = %02X\n", data);
        break;
    case 0x0F:  // RESET
        LC8951_LOG(LOG_INFO, "LC8951 RESET = %02X\n", data);
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
            controller.processCdCommand();
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
    out << lc8951.controller.status;
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
    in >> lc8951.controller.status;
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

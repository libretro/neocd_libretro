#ifndef LC8951_H
#define LC8951_H

#include <cstdint>

#include "datapacker.h"
#include "cdromcontroller.h"

class LC8951
{
public:
    enum IFCTRL_BITS
    {
        CMDIEN      = 0x80,
        DTEIEN      = 0x40,
        DECIEN      = 0x20,
        CMDBK       = 0x10,
        DTWAI       = 0x08,
        STWAI       = 0x04,
        DOUTEN      = 0x02,
        SOUTEN      = 0x01
    };

    enum IFSTAT_BITS
    {
        CMDI        = 0x80,
        DTEI        = 0x40,
        DECI        = 0x20,
        SUBI        = 0x10,
        DTBSY       = 0x08,
        STBSY       = 0x04,
        DTEN        = 0x02,
        STEN        = 0x01
    };

    enum CTRL0_BITS
    {
        DECEN       = 0x80,
        LOOKAHEAD   = 0x40,
        E01RQ       = 0x20,
        AUTORQ      = 0x10,
        ERAMRQ      = 0x08,
        WRRQ        = 0x04,
        ECCRQ       = 0x02,
        ENCODE      = 0x01
    };

    enum CTRL1_BITS
    {
        SYIEN       = 0x80,
        SYDEN       = 0x40,
        DSCREN      = 0x20,
        COWREN      = 0x10,
        MODRQ       = 0x08,
        FORMRQ      = 0x04,
        MBCKRQ      = 0x02,
        SHDREN      = 0x01
    };

    enum STAT0_BITS
    {
        CRCOK       = 0x80
    };

    LC8951();

    void reset();

    void updateHeadRegisters(uint32_t lba);

    uint8_t readRegister();
    void writeRegister(uint8_t data);
    void incrementRegisterPointer();
    void setRegisterPointer(uint8_t value);

    uint8_t readResponsePacket();
    void writeCommandPacket(uint8_t data);
    void increasePacketPointer(uint8_t data);
    void resetPacketPointers();

    inline uint16_t wordRegister(const uint8_t& low, const uint8_t& high) const
    {
         return (static_cast<uint16_t>(high) << 8) | static_cast<uint16_t>(low);
    }

    inline void setWordRegister(uint8_t& low, uint8_t& high, const uint16_t& value)
    {
        high = value >> 8;
        low = value;
    }

    inline void addWordRegister(uint8_t& low, uint8_t& high, uint16_t value)
    {
        value += wordRegister(low, high);
        setWordRegister(low, high, value);
    }

    void sectorDecoded();

    void endTransfer();

    static uint8_t calculatePacketChecksum(const uint8_t* packet);
    static void setPacketChecksum(uint8_t* packet);

    friend DataPacker& operator<<(DataPacker& out, const LC8951& lc8951);
    friend DataPacker& operator>>(DataPacker& in, LC8951& lc8951);

    // Variables to save in savestate
    CdromController controller;
    uint32_t    registerPointer;
    uint8_t     commandPacket[5];
    uint32_t    commandPointer;
    uint8_t     responsePacket[5];
    uint32_t    responsePointer;
    uint32_t    strobe;

    uint8_t     SBOUT;      // Status Byte Output
    uint8_t     IFCTRL;     // Interface Control ?
    uint8_t     DBCL;       // Data Byte Count Low
    uint8_t     DBCH;       // Data Byte Count High
    uint8_t     DACL;       // Data Address Counter Low
    uint8_t     DACH;       // Data Address Counter High
    uint8_t     DTRG;       // Data Transfer Trigger
    uint8_t     DTACK;      // Data Transfer Acknowledge
    uint8_t     WAL;        // Write Address Low
    uint8_t     WAH;        // Write Address High
    uint8_t     CTRL0;      // Control 0
    uint8_t     CTRL1;      // Control 1
    uint8_t     PTL;        // Pointer Low
    uint8_t     PTH;        // Pointer High

    uint8_t     COMIN;      // Command Input Register
    uint8_t     IFSTAT;     // Interface Status ?
    uint8_t     HEAD0;      // Minutes
    uint8_t     HEAD1;      // Seconds
    uint8_t     HEAD2;      // Frames
    uint8_t     HEAD3;      // Mode
    uint8_t     STAT0;      // Status 0
    uint8_t     STAT1;      // Status 1
    uint8_t     STAT2;      // Status 2
    uint8_t     STAT3;      // Status 3

    uint8_t     buffer[2048];
    // End variables to save in savestate
};

DataPacker& operator<<(DataPacker& out, const LC8951& lc8951);
DataPacker& operator>>(DataPacker& in, LC8951& lc8951);

#endif // LC8951_H

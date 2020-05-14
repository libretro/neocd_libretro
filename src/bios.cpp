#include <algorithm>
#include <cstring>

#include "bios.h"
#include "neocd_endian.h"
#include "misc.h"

// The patches below are just for convenience / speed as the emulator can run a BIOS unmodified.

// Patter data  used to identify BIOSes

static const uint8_t VALIDITY_SEARCH_PATTERN_DATA[] = { 0x00, 0x10, 0xF3, 0x00 };

static const uint8_t FRONT_LOADER_SEARCH_PATTERN_DATA[] = { 0x00, 0xC0, 0xC8, 0x5E };
static const uint8_t TOP_LOADER_SEARCH_PATTERN_DATA[] = { 0x00, 0xC0, 0xC2, 0x22 };
static const uint8_t CDZ_SEARCH_PATTERN_DATA[] = { 0x00, 0xC0, 0xA3, 0xE8 };

static const uint8_t SMKDAN_FRONT_SEARCH_PATTERN_DATA[] = { 0x00, 0xC2, 0x33, 0x00 };
static const uint8_t SMKDAN_TOP_SEARCH_PATTERN_DATA[] = { 0x00, 0xC2, 0x34, 0x00 };
static const uint8_t SMKDAN_CDZ_SEARCH_PATTERN_DATA[] = { 0x00, 0xC6, 0x20, 0x00 };

static const uint8_t UNIVERSE32_SEARCH_PATTERN_DATA[] = { 0x1C, 0xCA, 0x85, 0x8A };
static const uint8_t UNIVERSE33_SEARCH_PATTERN_DATA[] = { 0xA4, 0x4B, 0x15, 0x2F };

// Pattern data used for verfication before patching

static const uint8_t CD_REC_SEARCH_PATTERN_DATA_A[] = { 0x66, 0x10 };
static const uint8_t CD_REC_SEARCH_PATTERN_DATA_B[] = { 0x66, 0x74 };
static const uint8_t CD_REC_SEARCH_PATTERN_DATA_C[] = { 0x66, 0x04 };

static const uint8_t SPEEDHACK_SEARCH_PATTERN_DATA_A[] = { 0x53, 0x81, 0x67, 0x00, 0xFE, 0xF4 };
static const uint8_t SPEEDHACK_SEARCH_PATTERN_DATA_B[] = { 0x53, 0x81, 0x67, 0x00, 0x00, 0x0E };
static const uint8_t SPEEDHACK_SEARCH_PATTERN_DATA_C[] = { 0x53, 0x81, 0x67, 0x00, 0xFE, 0x70 };
static const uint8_t SPEEDHACK_SEARCH_PATTERN_DATA_D[] = { 0x53, 0x81, 0x67, 0x00, 0xFF, 0x46 };
static const uint8_t SPEEDHACK_SEARCH_PATTERN_DATA_E[] = { 0x53, 0x81, 0x67, 0x00, 0xFE, 0xC4 };
static const uint8_t SPEEDHACK_SEARCH_PATTERN_DATA_F[] = { 0x53, 0x81, 0x67, 0x00, 0xFF, 0x2A };
static const uint8_t SPEEDHACK_SEARCH_PATTERN_DATA_G[] = { 0x53, 0x81, 0x67, 0x00, 0xFE, 0xA6 };

static const uint8_t UNIBIOS33_CHECKSUM_SEARCH_PATTERN_DATA[] = { 0x67, 0x32 };

static const uint8_t SMKDAN_CHECKSUM_SEARCH_PATTERN_DATA_A[] = { 0x22, 0x39, 0x00, 0xC6,  0xFF,  0xF4 };
static const uint8_t SMKDAN_CHECKSUM_SEARCH_PATTERN_DATA_B[] = { 0x22, 0x39, 0x00, 0xC2,  0xFF,  0xF4 };

// Replace data for patches

static const uint8_t REPLACE_DATA_NOP[] = { 0x4E, 0x71 };
static const uint8_t REPLACE_DATA_SPEEDHACK[] =  { 0xFA, 0xBE, 0x4E, 0x71, 0x4E, 0x71 };
static const uint8_t REPLACE_DATA_UNIBIOS33_CHECKSUM[] =  { 0x60, 0x32 };
static const uint8_t REPLACE_DATA_SMKDAN_CHECKSUM[] = { 0x22, 0x00, 0x4E, 0x71, 0x4E, 0x71 };

// Search patterns for BIOS identification

static const Bios::Pattern VALIDITY_SEARCH_PATTERN { 0xC00000, VALIDITY_SEARCH_PATTERN_DATA, sizeof(VALIDITY_SEARCH_PATTERN_DATA) };
static const Bios::Pattern FRONT_LOADER_SEARCH_PATTERN { 0xC0006C, FRONT_LOADER_SEARCH_PATTERN_DATA, sizeof(FRONT_LOADER_SEARCH_PATTERN_DATA) };
static const Bios::Pattern TOP_LOADER_SEARCH_PATTERN { 0xC0006C, TOP_LOADER_SEARCH_PATTERN_DATA, sizeof(TOP_LOADER_SEARCH_PATTERN_DATA) };
static const Bios::Pattern CDZ_SEARCH_PATTERN { 0xC0006C, CDZ_SEARCH_PATTERN_DATA, sizeof(CDZ_SEARCH_PATTERN_DATA) };
static const Bios::Pattern SMKDAN_FRONT_SEARCH_PATTERN { 0xC00004, SMKDAN_FRONT_SEARCH_PATTERN_DATA, sizeof(SMKDAN_FRONT_SEARCH_PATTERN_DATA) };
static const Bios::Pattern SMKDAN_TOP_SEARCH_PATTERN { 0xC00004, SMKDAN_TOP_SEARCH_PATTERN_DATA, sizeof(SMKDAN_TOP_SEARCH_PATTERN_DATA) };
static const Bios::Pattern SMKDAN_CDZ_SEARCH_PATTERN { 0xC00004, SMKDAN_CDZ_SEARCH_PATTERN_DATA, sizeof(SMKDAN_CDZ_SEARCH_PATTERN_DATA) };
static const Bios::Pattern UNIVERSE32_SEARCH_PATTERN { 0xC00150, UNIVERSE32_SEARCH_PATTERN_DATA, sizeof(UNIVERSE32_SEARCH_PATTERN_DATA) };
static const Bios::Pattern UNIVERSE33_SEARCH_PATTERN { 0xC00150, UNIVERSE33_SEARCH_PATTERN_DATA, sizeof(UNIVERSE33_SEARCH_PATTERN_DATA) };

// Replace patterns for patches

static const Bios::ReplacePattern CDZ_CD_RECOG_REPLACE[] = {
    { 0xC0EB82, CD_REC_SEARCH_PATTERN_DATA_A, REPLACE_DATA_NOP, sizeof(CD_REC_SEARCH_PATTERN_DATA_A) },
    { 0xC0D280, CD_REC_SEARCH_PATTERN_DATA_B, REPLACE_DATA_NOP, sizeof(CD_REC_SEARCH_PATTERN_DATA_B) },
    { 0x000000, nullptr, nullptr, 0 }
};

static const Bios::ReplacePattern CDZ_SPEEDHACK_REPLACE[] = {
    { 0xC0E6E0, SPEEDHACK_SEARCH_PATTERN_DATA_A, REPLACE_DATA_SPEEDHACK, sizeof(SPEEDHACK_SEARCH_PATTERN_DATA_A) },
    { 0xC0E724, SPEEDHACK_SEARCH_PATTERN_DATA_B, REPLACE_DATA_SPEEDHACK, sizeof(SPEEDHACK_SEARCH_PATTERN_DATA_B) },
    { 0xC0E764, SPEEDHACK_SEARCH_PATTERN_DATA_C, REPLACE_DATA_SPEEDHACK, sizeof(SPEEDHACK_SEARCH_PATTERN_DATA_C) },
    { 0xC0E836, SPEEDHACK_SEARCH_PATTERN_DATA_B, REPLACE_DATA_SPEEDHACK, sizeof(SPEEDHACK_SEARCH_PATTERN_DATA_B) },
    { 0xC0E860, SPEEDHACK_SEARCH_PATTERN_DATA_B, REPLACE_DATA_SPEEDHACK, sizeof(SPEEDHACK_SEARCH_PATTERN_DATA_B) },
    { 0x000000, nullptr, nullptr, 0 }
};

static const Bios::ReplacePattern CDZ_UNIVERSE33_CHECKSUM_REPLACE[] = {
    { 0xC1D3EC, UNIBIOS33_CHECKSUM_SEARCH_PATTERN_DATA, REPLACE_DATA_UNIBIOS33_CHECKSUM, sizeof(UNIBIOS33_CHECKSUM_SEARCH_PATTERN_DATA) },
    { 0x000000, nullptr, nullptr, 0 }
};

static const Bios::ReplacePattern CDZ_SMKDAN_CHECKSUM_REPLACE[] = {
    { 0xC62BF4, SMKDAN_CHECKSUM_SEARCH_PATTERN_DATA_A, REPLACE_DATA_SMKDAN_CHECKSUM, sizeof(SMKDAN_CHECKSUM_SEARCH_PATTERN_DATA_A) },
    { 0x000000, nullptr, nullptr, 0 }
};

static const Bios::ReplacePattern FRONT_CD_RECOG_REPLACE[] = {
    { 0xC10B64, CD_REC_SEARCH_PATTERN_DATA_C, REPLACE_DATA_NOP, sizeof(CD_REC_SEARCH_PATTERN_DATA_C) },
    { 0x000000, nullptr, nullptr, 0 }
};

static const Bios::ReplacePattern FRONT_SPEEDHACK_REPLACE[] = {
    { 0xC10716, SPEEDHACK_SEARCH_PATTERN_DATA_D, REPLACE_DATA_SPEEDHACK, sizeof(SPEEDHACK_SEARCH_PATTERN_DATA_D) },
    { 0xC10758, SPEEDHACK_SEARCH_PATTERN_DATA_B, REPLACE_DATA_SPEEDHACK, sizeof(SPEEDHACK_SEARCH_PATTERN_DATA_B) },
    { 0xC10798, SPEEDHACK_SEARCH_PATTERN_DATA_E, REPLACE_DATA_SPEEDHACK, sizeof(SPEEDHACK_SEARCH_PATTERN_DATA_E) },
    { 0xC10864, SPEEDHACK_SEARCH_PATTERN_DATA_B, REPLACE_DATA_SPEEDHACK, sizeof(SPEEDHACK_SEARCH_PATTERN_DATA_B) },
    { 0x000000, nullptr, nullptr, 0 }
};

static const Bios::ReplacePattern FRONT_SMKDAN_CHECKSUM_REPLACE[] = {
    { 0xC23EBE, SMKDAN_CHECKSUM_SEARCH_PATTERN_DATA_B, REPLACE_DATA_SMKDAN_CHECKSUM, sizeof(SMKDAN_CHECKSUM_SEARCH_PATTERN_DATA_B) },
    { 0x000000, nullptr, nullptr, 0 }
};

static const Bios::ReplacePattern TOP_CD_RECOG_REPLACE[] = {
    { 0xC10436, CD_REC_SEARCH_PATTERN_DATA_C, REPLACE_DATA_NOP, sizeof(CD_REC_SEARCH_PATTERN_DATA_C) },
    { 0x000000, nullptr, nullptr, 0 }
};

static const Bios::ReplacePattern TOP_SPEEDHACK_REPLACE[] = {
    { 0xC0FFCA, SPEEDHACK_SEARCH_PATTERN_DATA_F, REPLACE_DATA_SPEEDHACK, sizeof(SPEEDHACK_SEARCH_PATTERN_DATA_F) },
    { 0xC1000E, SPEEDHACK_SEARCH_PATTERN_DATA_B, REPLACE_DATA_SPEEDHACK, sizeof(SPEEDHACK_SEARCH_PATTERN_DATA_B) },
    { 0xC1004E, SPEEDHACK_SEARCH_PATTERN_DATA_G, REPLACE_DATA_SPEEDHACK, sizeof(SPEEDHACK_SEARCH_PATTERN_DATA_G) },
    { 0xC10120, SPEEDHACK_SEARCH_PATTERN_DATA_B, REPLACE_DATA_SPEEDHACK, sizeof(SPEEDHACK_SEARCH_PATTERN_DATA_B) },
    { 0x000000, nullptr, nullptr, 0 }
};

static const Bios::ReplacePattern TOP_SMKDAN_CHECKSUM_REPLACE[] = {
    { 0xC23FBE, SMKDAN_CHECKSUM_SEARCH_PATTERN_DATA_B, REPLACE_DATA_SMKDAN_CHECKSUM, sizeof(SMKDAN_CHECKSUM_SEARCH_PATTERN_DATA_B) },
    { 0x000000, nullptr, nullptr, 0 }
};

// Error messages
static const char* const MSG_CD_RECOG_PATCH_FAILED = "BIOS: CD recognition patch failed.\n";
static const char* const MSG_CD_SPEEDHACK_PATCH_FAILED = "BIOS: Speed hack patch failed.\n";
static const char* const MSG_CD_SMKDAN_CRC_PATCH_FAILED = "BIOS: SMKDAN checksum patch failed.\n";
static const char* const MSG_CD_UNIVERSE33_CRC_PATCH_FAILED = "WARNING: BIOS Universe 3.3 checksum patch failed.\n";

void Bios::autoByteSwap(uint8_t *biosData)
{
    // Swap the BIOS if needed
    if (*reinterpret_cast<uint16_t*>(&biosData[0]) == LITTLE_ENDIAN_WORD(0x0010))
    {
        uint16_t* start = reinterpret_cast<uint16_t*>(&biosData[0]);
        uint16_t* end = reinterpret_cast<uint16_t*>(&biosData[/*Memory::ROM_SIZE*/ 0x80000]);

        std::for_each(start, end, [](uint16_t& data) {
            data = BYTE_SWAP_16(data);
        });
    }
}

Bios::Type Bios::identify(const uint8_t *biosData)
{
    Bios::Family family = Bios::Family::Invalid;
    Bios::Mod mod = Bios::Mod::None;

    if (isPatternPresent(biosData, VALIDITY_SEARCH_PATTERN))
        family = Bios::Unknown;

    if (isPatternPresent(biosData, FRONT_LOADER_SEARCH_PATTERN))
    {
        family = Bios::FrontLoader;

        if (isPatternPresent(biosData, SMKDAN_FRONT_SEARCH_PATTERN))
            mod = Bios::SMKDan;
    }
    else if (isPatternPresent(biosData, TOP_LOADER_SEARCH_PATTERN))
    {
        family = Bios::TopLoader;

        if (isPatternPresent(biosData, SMKDAN_TOP_SEARCH_PATTERN))
            mod = Bios::SMKDan;
    }
    else if (isPatternPresent(biosData, CDZ_SEARCH_PATTERN))
    {
        family = Bios::CDZ;

        if (isPatternPresent(biosData, SMKDAN_CDZ_SEARCH_PATTERN))
            mod = Bios::SMKDan;
        else if (isPatternPresent(biosData, UNIVERSE32_SEARCH_PATTERN))
            mod = Bios::Universe32;
        else if (isPatternPresent(biosData, UNIVERSE33_SEARCH_PATTERN))
            mod = Bios::Universe33;
    }

    return std::make_pair(family, mod);
}

void Bios::patch(uint8_t *biosData, const Bios::Type biosType, bool speedHackEnabled)
{
    if (biosType.first == Bios::CDZ)
    {
        if (!replacePattern(biosData, CDZ_CD_RECOG_REPLACE))
            LOG(LOG_ERROR, MSG_CD_RECOG_PATCH_FAILED);

        if (speedHackEnabled)
        {
            if (!replacePattern(biosData, CDZ_SPEEDHACK_REPLACE))
                LOG(LOG_ERROR, MSG_CD_SPEEDHACK_PATCH_FAILED);
        }

        if (biosType.second == Bios::SMKDan)
        {
            if (!replacePattern(biosData, CDZ_SMKDAN_CHECKSUM_REPLACE))
                LOG(LOG_ERROR, MSG_CD_SMKDAN_CRC_PATCH_FAILED);
        }

        if (biosType.second == Bios::Universe33)
        {
            if (!replacePattern(biosData, CDZ_UNIVERSE33_CHECKSUM_REPLACE))
                LOG(LOG_ERROR, MSG_CD_UNIVERSE33_CRC_PATCH_FAILED);
        }
    }
    else if (biosType.first == Bios::FrontLoader)
    {
        if (!replacePattern(biosData, FRONT_CD_RECOG_REPLACE))
            LOG(LOG_ERROR, MSG_CD_RECOG_PATCH_FAILED);

        if (speedHackEnabled)
        {
            if (!replacePattern(biosData, FRONT_SPEEDHACK_REPLACE))
                LOG(LOG_ERROR, MSG_CD_SPEEDHACK_PATCH_FAILED);
        }

        if (biosType.second == Bios::SMKDan)
        {
            if (!replacePattern(biosData, FRONT_SMKDAN_CHECKSUM_REPLACE))
                LOG(LOG_ERROR, MSG_CD_SMKDAN_CRC_PATCH_FAILED);
        }
    }
    else if (biosType.first == Bios::TopLoader)
    {
        if (!replacePattern(biosData, TOP_CD_RECOG_REPLACE))
            LOG(LOG_ERROR, MSG_CD_RECOG_PATCH_FAILED);

        if (speedHackEnabled)
        {
            if (!replacePattern(biosData, TOP_SPEEDHACK_REPLACE))
                LOG(LOG_ERROR, MSG_CD_SPEEDHACK_PATCH_FAILED);
        }

        if (biosType.second == Bios::SMKDan)
        {
            if (!replacePattern(biosData, TOP_SMKDAN_CHECKSUM_REPLACE))
                LOG(LOG_ERROR, MSG_CD_SMKDAN_CRC_PATCH_FAILED);
        }
    }
}

std::string Bios::description(Bios::Type type)
{
    std::string result;

    switch(type.first)
    {
    case Bios::Family::Invalid:
        result = "Invalid";
        break;

    case Bios::Family::Unknown:
        result = "Unknown";
        break;

    case Bios::Family::FrontLoader:
        result = "Front Loader";
        break;

    case Bios::Family::TopLoader:
        result = "Top Loader";
        break;

    case Bios::Family::CDZ:
        result = "CDZ";
        break;
    }

    switch(type.second)
    {
    case Bios::Mod::None:
        break;

    case Bios::Mod::SMKDan:
        result.append(", SMKDan");
        break;

    case Bios::Mod::Universe32:
        result.append(", Universe 3.2");
        break;

    case Bios::Mod::Universe33:
        result.append(", Universe 3.3");
        break;
    }

    return result;
}

bool Bios::isPatternPresent(const uint8_t *biosData, const Bios::Pattern &pattern)
{
    return (std::memcmp(pattern.data, biosData + pattern.address - 0xc00000, pattern.length) == 0);
}

bool Bios::replacePattern(uint8_t *biosData, const ReplacePattern *replacePattern)
{
    const ReplacePattern* ptr = replacePattern;

    while(ptr->address)
    {
        if (std::memcmp(ptr->originalData, biosData + ptr->address - 0xC00000, ptr->length) != 0)
            return false;

        ++ptr;
    }

    ptr = replacePattern;

    while(ptr->address)
    {
        std::memcpy(biosData + ptr->address - 0xC00000, ptr->modifiedData, ptr->length);
        ++ptr;
    }

    return true;
}

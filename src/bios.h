#ifndef BIOS_H
#define BIOS_H

#include <cstdint>
#include <string>
#include <utility>

class Bios
{
public:
    enum Family : uint8_t
    {
        FrontLoader = 0,
        TopLoader = 1,
        CDZ = 2,
        Invalid = 254,
        Unknown = 255
    };

    enum Mod : uint8_t
    {
        None,
        SMKDan,
        SMKDanBeta,
        Universe32,
        Universe33
    };

    using Type = std::pair<Family, Mod>;

    struct Pattern
    {
        uint32_t address;
        const uint8_t* data;
        uint32_t length;
    };


    struct ReplacePattern
    {
        uint32_t address;
        const uint8_t* originalData;
        const uint8_t* modifiedData;
        uint32_t length;
    };

    explicit Bios() = delete;

    static void autoByteSwap(uint8_t* biosData);

    static Type identify(const uint8_t* biosData);

    static void patch(uint8_t* biosData, const Type biosType, bool speedHackEnabled);

    static std::string description(Bios::Type type);

protected:
    static bool isPatternPresent(const uint8_t* biosData, const Pattern& pattern);

    static bool replacePattern(uint8_t* biosData, const ReplacePattern* replacePattern);
};

#endif // BIOS_H

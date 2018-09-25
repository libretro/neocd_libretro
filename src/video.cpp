#include "neogeocd.h"
#include "video.h"
#include "endian.h"
#include "always_inline.h"

#include <cstring>
#include <algorithm>

static const uint32_t SPR_DECODE_TABLE[256] =
{
    0x00000000, 0x00000001, 0x00000010, 0x00000011,
    0x00000100, 0x00000101, 0x00000110, 0x00000111,
    0x00001000, 0x00001001, 0x00001010, 0x00001011,
    0x00001100, 0x00001101, 0x00001110, 0x00001111,
    0x00010000, 0x00010001, 0x00010010, 0x00010011,
    0x00010100, 0x00010101, 0x00010110, 0x00010111,
    0x00011000, 0x00011001, 0x00011010, 0x00011011,
    0x00011100, 0x00011101, 0x00011110, 0x00011111,
    0x00100000, 0x00100001, 0x00100010, 0x00100011,
    0x00100100, 0x00100101, 0x00100110, 0x00100111,
    0x00101000, 0x00101001, 0x00101010, 0x00101011,
    0x00101100, 0x00101101, 0x00101110, 0x00101111,
    0x00110000, 0x00110001, 0x00110010, 0x00110011,
    0x00110100, 0x00110101, 0x00110110, 0x00110111,
    0x00111000, 0x00111001, 0x00111010, 0x00111011,
    0x00111100, 0x00111101, 0x00111110, 0x00111111,
    0x01000000, 0x01000001, 0x01000010, 0x01000011,
    0x01000100, 0x01000101, 0x01000110, 0x01000111,
    0x01001000, 0x01001001, 0x01001010, 0x01001011,
    0x01001100, 0x01001101, 0x01001110, 0x01001111,
    0x01010000, 0x01010001, 0x01010010, 0x01010011,
    0x01010100, 0x01010101, 0x01010110, 0x01010111,
    0x01011000, 0x01011001, 0x01011010, 0x01011011,
    0x01011100, 0x01011101, 0x01011110, 0x01011111,
    0x01100000, 0x01100001, 0x01100010, 0x01100011,
    0x01100100, 0x01100101, 0x01100110, 0x01100111,
    0x01101000, 0x01101001, 0x01101010, 0x01101011,
    0x01101100, 0x01101101, 0x01101110, 0x01101111,
    0x01110000, 0x01110001, 0x01110010, 0x01110011,
    0x01110100, 0x01110101, 0x01110110, 0x01110111,
    0x01111000, 0x01111001, 0x01111010, 0x01111011,
    0x01111100, 0x01111101, 0x01111110, 0x01111111,
    0x10000000, 0x10000001, 0x10000010, 0x10000011,
    0x10000100, 0x10000101, 0x10000110, 0x10000111,
    0x10001000, 0x10001001, 0x10001010, 0x10001011,
    0x10001100, 0x10001101, 0x10001110, 0x10001111,
    0x10010000, 0x10010001, 0x10010010, 0x10010011,
    0x10010100, 0x10010101, 0x10010110, 0x10010111,
    0x10011000, 0x10011001, 0x10011010, 0x10011011,
    0x10011100, 0x10011101, 0x10011110, 0x10011111,
    0x10100000, 0x10100001, 0x10100010, 0x10100011,
    0x10100100, 0x10100101, 0x10100110, 0x10100111,
    0x10101000, 0x10101001, 0x10101010, 0x10101011,
    0x10101100, 0x10101101, 0x10101110, 0x10101111,
    0x10110000, 0x10110001, 0x10110010, 0x10110011,
    0x10110100, 0x10110101, 0x10110110, 0x10110111,
    0x10111000, 0x10111001, 0x10111010, 0x10111011,
    0x10111100, 0x10111101, 0x10111110, 0x10111111,
    0x11000000, 0x11000001, 0x11000010, 0x11000011,
    0x11000100, 0x11000101, 0x11000110, 0x11000111,
    0x11001000, 0x11001001, 0x11001010, 0x11001011,
    0x11001100, 0x11001101, 0x11001110, 0x11001111,
    0x11010000, 0x11010001, 0x11010010, 0x11010011,
    0x11010100, 0x11010101, 0x11010110, 0x11010111,
    0x11011000, 0x11011001, 0x11011010, 0x11011011,
    0x11011100, 0x11011101, 0x11011110, 0x11011111,
    0x11100000, 0x11100001, 0x11100010, 0x11100011,
    0x11100100, 0x11100101, 0x11100110, 0x11100111,
    0x11101000, 0x11101001, 0x11101010, 0x11101011,
    0x11101100, 0x11101101, 0x11101110, 0x11101111,
    0x11110000, 0x11110001, 0x11110010, 0x11110011,
    0x11110100, 0x11110101, 0x11110110, 0x11110111,
    0x11111000, 0x11111001, 0x11111010, 0x11111011,
    0x11111100, 0x11111101, 0x11111110, 0x11111111
};

Video::Video() :
    paletteRamPc(nullptr),
    fixUsageMap(nullptr),
    frameBuffer(nullptr),
    activePaletteBank(0),
    autoAnimationCounter(0),
    autoAnimationFrameCounter(0),
    autoAnimationSpeed(0),
    autoAnimationDisabled(false),
    sprDisable(true),
    fixDisable(true),
    videoEnable(false),
    hirqControl(HIRQ_CTRL_DISABLE),
    hirqRegister(0),
    videoramOffset(0),
    videoramModulo(0),
    videoramData(0),
    sprite_x(0),
    sprite_y(0),
    sprite_zoomX(15),
    sprite_zoomY(255),
    sprite_clipping(0x20)
{
    // Palette RAM: 16KiB, converted into RGB565 equivalent
    paletteRamPc = reinterpret_cast<uint16_t*>(std::malloc(Memory::PALETTERAM_SIZE));

    // Fix usage map, if zero tile is fully transparent
    fixUsageMap = reinterpret_cast<uint8_t*>(std::malloc(Memory::FIXRAM_SIZE / 32));

    // 304x224 RGB565 framebuffer
    frameBuffer = reinterpret_cast<uint16_t*>(std::malloc(FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT * sizeof(uint16_t)));
}

Video::~Video()
{
    if (frameBuffer)
        std::free(frameBuffer);

    if (fixUsageMap)
        std::free(fixUsageMap);

    if (paletteRamPc)
        std::free(paletteRamPc);
}

void Video::reset()
{
    std::memset(paletteRamPc, 0, Memory::PALETTERAM_SIZE);
    std::memset(fixUsageMap, 0, Memory::FIXRAM_SIZE / 32);
    activePaletteBank = 0;
    autoAnimationCounter = 0;
    autoAnimationFrameCounter = 0;
    autoAnimationSpeed = 0;
    autoAnimationDisabled = false;
    sprDisable = true;
    fixDisable = true;
    videoEnable = false;
    hirqControl = HIRQ_CTRL_DISABLE;
    hirqRegister = 0;
    videoramOffset = 0;
    videoramModulo = 0;
    videoramData = 0;
    sprite_x = 0;
    sprite_y = 0;
    sprite_zoomX = 15;
    sprite_zoomY = 255;
    sprite_clipping = 0x20;
}

void Video::convertColor(uint32_t index)
{
    uint16_t c = BIG_ENDIAN_WORD(neocd.memory.paletteRam[index]);

    paletteRamPc[index] = ((c & 0x0F00) << 4) | ((c & 0x4000) >> 3) |
        ((c & 0x00F0) << 3) | ((c & 0x2000) >> 7) |
        ((c & 0x000F) << 1) | ((c & 0x1000) >> 12);
}

void Video::convertPalette()
{
    for (int i = 0; i < (Memory::PALETTERAM_SIZE / sizeof(uint16_t)); ++i)
        convertColor(i);
}

void Video::updateFixUsageMap()
{
    uint8_t* fixPtr = neocd.memory.fixRam;
    uint8_t* usagePtrStart = fixUsageMap;
    uint8_t* usagePtrEnd = fixUsageMap + (Memory::FIXRAM_SIZE / 32);

    std::generate(usagePtrStart, usagePtrEnd, [&]() {
        bool result = std::any_of(fixPtr, fixPtr + 32, [](const uint8_t& value) {
            return value ? 1 : 0;
        });

        fixPtr += 32;
        return result;
    });
}

// Note: scanline between 16 and 240!
void Video::drawFix(uint32_t scanline)
{
    // Skip the first column (framebuffer width is 304 instead of 320)
    uint16_t* videoRamPtr = &neocd.memory.videoRam[(0xE004 / 2) + ((scanline - 16) / 8)] + 32;
    uint16_t* videoRamEndPtr = videoRamPtr + (FRAMEBUFFER_WIDTH * 4);
    uint16_t* frameBufferPtr = frameBuffer + ((scanline - 16) * FRAMEBUFFER_WIDTH);

    for (; videoRamPtr < videoRamEndPtr; videoRamPtr += 32)
    {
        int character = (*videoRamPtr) & 0x0FFF;
        int palette = ((*videoRamPtr) & 0xF000) >> 12;

        // Check for total transparency, no need to draw
        if (!fixUsageMap[character])
        {
            frameBufferPtr += 8;
            continue;
        }

        uint8_t* fixBase = &neocd.memory.fixRam[(character * 32) + (scanline % 8)];
        uint16_t* paletteBase = &paletteRamPc[(activePaletteBank * 4096) + (palette * 16)];

        auto decodeAndDrawFix = [&](int n) ALWAYS_INLINE {
            uint8_t pixelA = fixBase[n];
            uint8_t pixelB = pixelA >> 4;
            pixelA &= 0x0F;

            if (pixelA)
                *frameBufferPtr = paletteBase[pixelA];
            frameBufferPtr++;

            if (pixelB)
                *frameBufferPtr = paletteBase[pixelB];
            frameBufferPtr++;
        };

        decodeAndDrawFix(16);
        decodeAndDrawFix(24);
        decodeAndDrawFix(0);
        decodeAndDrawFix(8);
    }
}

static inline bool isSpriteOnScanline(uint32_t scanline, uint32_t y, uint32_t clipping)
{
    return (clipping == 0) || (clipping >= 0x20) || ((scanline - y) & 0x1ff) < (clipping * 0x10);
}

uint16_t Video::createSpriteList(uint32_t scanline, uint16_t *spriteList) const
{
    uint16_t* attributesPtr = &neocd.memory.videoRam[0x8200];
    uint16_t activeCount = 0;
    uint16_t attributes;
    uint32_t y = sprite_y;
    uint32_t clipping = sprite_clipping;
    bool spriteIsOnScanline = false;

    for (uint16_t spriteNumber = 0; spriteNumber < MAX_SPRITES_PER_SCREEN; ++spriteNumber)
    {
        attributes = *attributesPtr++;

        if (!(attributes & 0x40))
        {
            y = 0x200 - (attributes >> 7);
            clipping = attributes & 0x3F;
            spriteIsOnScanline = isSpriteOnScanline(scanline, y, clipping);
        }

        if (!spriteIsOnScanline)
            continue;

        if (!clipping)
            continue;

        *spriteList++ = spriteNumber;
        activeCount++;

        if (activeCount >= MAX_SPRITES_PER_LINE)
            break;
    }

    // Fill the rest of the sprite list with 0, including one extra entry
    std::memset(spriteList, 0, sizeof(uint16_t) * (MAX_SPRITES_PER_LINE - activeCount + 1));

    return activeCount;
}

void Video::drawSprites(uint32_t scanline, uint16_t *spriteList, uint16_t spritesToDraw)
{
    for (uint16_t currentSprite = 0; currentSprite <= spritesToDraw; currentSprite++)
    {
        uint16_t spriteNumber = *spriteList++;
        // Optimization: No need to draw sprite zero now if we're going to draw it again in the end
        if ((!spriteNumber) && (currentSprite < spritesToDraw))
            continue;

        uint16_t spriteAttributes1 = neocd.memory.videoRam[0x8000 + spriteNumber];
        uint16_t spriteAttributes2 = neocd.memory.videoRam[0x8200 + spriteNumber];
        uint16_t spriteAttributes3 = neocd.memory.videoRam[0x8400 + spriteNumber];

        if (spriteAttributes2 & 0x40)
        {
            sprite_x = (sprite_x + sprite_zoomX + 1) & 0x1FF;
            sprite_zoomX = (spriteAttributes1 >> 8) & 0xF;
        }
        else
        {
            sprite_zoomY = spriteAttributes1 & 0xFF;
            sprite_zoomX = (spriteAttributes1 >> 8) & 0xF;
            sprite_clipping = spriteAttributes2 & 0x3F;
            sprite_y = 0x200 - (spriteAttributes2 >> 7);
            sprite_x = spriteAttributes3 >> 7;
        }

        if ((sprite_x >= 0x138) && (sprite_x <= 0x1F8))
            continue;

        drawSprite(spriteNumber, sprite_x, sprite_y, sprite_zoomX, sprite_zoomY, scanline, sprite_clipping);
    }
}

inline void drawSpriteLine(
    uint32_t zoomX,
    int increment,
    uint32_t pixelData,
    uint32_t pixelDataB,
    const uint16_t* paletteBase,
    uint16_t*& frameBufferPtr)
{
    auto sprShift = [&](int n) {
        pixelData >>= (n * 4);
    };

    auto sprDrawPixel = [&]() {
        uint8_t pixel = pixelData & 0xF;
        if (pixel)
            *frameBufferPtr = paletteBase[pixel];
        frameBufferPtr += increment;
    };

    switch (zoomX)
    {
    case 0:     // 00000000
        break;

    case 1:     // 00001000
    case 2:
        sprShift(4);
        sprDrawPixel();
        break;

    case 3:     // 00101000
    case 4:
        sprShift(2);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        break;

    case 5:     // 00101010
    case 6:
        sprShift(2);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        break;

    case 7:     // 10101010
    case 8:
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        break;

    case 9:     // 10111010
    case 10:
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        break;

    case 11:    // 10111011
    case 12:
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        break;

    case 13:    // 11111011
    case 14:
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        break;

    case 15:    // 11111111
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        break;
    }

    pixelData = pixelDataB;

    switch (zoomX)
    {
    case    0:      // 10000000
    case    1:
        sprDrawPixel();
        break;

    case    2:      // 10001000
    case    3:
        sprDrawPixel();
        sprShift(4);
        sprDrawPixel();
        break;

    case    4:      // 10001010
    case    5:
        sprDrawPixel();
        sprShift(4);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        break;

    case    6:      // 10101010
    case    7:
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        break;

    case    8:      // 11101010
    case    9:
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        break;

    case    10:     // 11101011
    case    11:
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        break;

    case    12:     // 11101111
    case    13:
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        break;

    case    14:
    case    15:     // 11111111
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        break;
    }
}

inline void drawSpriteLineClipped(
    uint32_t zoomX,
    int increment,
    uint32_t pixelData,
    uint32_t pixelDataB,
    const uint16_t* paletteBase,
    uint16_t*& frameBufferPtr,
    const uint16_t* low,
    const uint16_t* high)
{
    auto sprShift = [&](int n) {
        pixelData >>= (n * 4);
    };

    auto sprDrawPixel = [&]() {
        uint8_t pixel = pixelData & 0xF;
        if (pixel && (frameBufferPtr >= low) && (frameBufferPtr < high))
            *frameBufferPtr = paletteBase[pixel];
        frameBufferPtr += increment;
    };

    switch (zoomX)
    {
    case 0:     // 00000000
        break;

    case 1:     // 00001000
    case 2:
        sprShift(4);
        sprDrawPixel();
        break;

    case 3:     // 00101000
    case 4:
        sprShift(2);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        break;

    case 5:     // 00101010
    case 6:
        sprShift(2);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        break;

    case 7:     // 10101010
    case 8:
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        break;

    case 9:     // 10111010
    case 10:
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        break;

    case 11:    // 10111011
    case 12:
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        break;

    case 13:    // 11111011
    case 14:
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        break;

    case 15:    // 11111111
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        break;
    }

    pixelData = pixelDataB;

    switch (zoomX)
    {
    case    0:      // 10000000
    case    1:
        sprDrawPixel();
        break;

    case    2:      // 10001000
    case    3:
        sprDrawPixel();
        sprShift(4);
        sprDrawPixel();
        break;

    case    4:      // 10001010
    case    5:
        sprDrawPixel();
        sprShift(4);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        break;

    case    6:      // 10101010
    case    7:
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        break;

    case    8:      // 11101010
    case    9:
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        break;

    case    10:     // 11101011
    case    11:
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        break;

    case    12:     // 11101111
    case    13:
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(2);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        break;

    case    14:
    case    15:     // 11111111
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        sprShift(1);
        sprDrawPixel();
        break;
    }
}

void Video::drawSprite(uint32_t spriteNumber, uint32_t x, uint32_t y, uint32_t zoomX, uint32_t zoomY, uint32_t scanline, uint32_t clipping)
{
    uint32_t spriteLine = (scanline - y) & 0x1FF;
    uint32_t zoomLine = spriteLine & 0xFF;
    bool invert = (spriteLine & 0x100) != 0;
    bool clippedDrawing;

    if (x >= 0x1F9)
    {
        if (x + zoomX + 1 > 0x208)
            clippedDrawing = true;
        else
            return;
    }
    else if (x < 8)
    {
        if (x + zoomX + 1 > 8)
            clippedDrawing = true;
        else
            return;
    }
    else if (x + zoomX + 1 > 0x138)
        clippedDrawing = true;
    else
        clippedDrawing = false;

    if (invert)
        zoomLine ^= 0xFF;

    if (clipping > 0x20)
    {
        zoomLine = zoomLine % ((zoomY + 1) << 1);

        if (zoomLine > zoomY)
        {
            zoomLine = ((zoomY + 1) << 1) - 1 - zoomLine;
            invert = !invert;
        }
    }

    uint32_t tileNumber = neocd.memory.yZoomRom[zoomY * 256 + zoomLine];
    uint32_t tileLine = tileNumber & 0xF;
    tileNumber >>= 4;

    if (invert)
    {
        tileLine ^= 0x0f;
        tileNumber ^= 0x1f;
    }

    uint32_t tileIndex = neocd.memory.videoRam[spriteNumber * 64 + tileNumber * 2];
    uint32_t tileControl = neocd.memory.videoRam[spriteNumber * 64 + tileNumber * 2 + 1];

    if (tileControl & 2)
        tileLine ^= 0x0F;

    // Auto animation
    if (!autoAnimationDisabled)
    {
        if (tileControl & 0x0008)
            tileIndex = (tileIndex & ~0x07) | (autoAnimationCounter & 0x07);
        else if (tileControl & 0x0004)
            tileIndex = (tileIndex & ~0x03) | (autoAnimationCounter & 0x03);
    }

    const uint16_t* paletteBase = &paletteRamPc[(activePaletteBank * 0x1000) + ((tileControl >> 8) * 16)];
    const uint8_t* spriteBase = &neocd.memory.sprRam[(tileIndex & 0x7FFF) * 128 + (tileLine * 4)];

    uint16_t* frameBufferPtr = frameBuffer;

    if (x > 0x1F0)
    {
        frameBufferPtr += x;
        frameBufferPtr -= 0x208;
    }
    else
    {
        frameBufferPtr += x;
        frameBufferPtr -= 8;
    }

    frameBufferPtr += (scanline - 16) * FRAMEBUFFER_WIDTH;

    int increment;

    if (tileControl & 1)
    {
        frameBufferPtr += zoomX;
        increment = -1;
    }
    else
        increment = 1;

    uint32_t pixelData = SPR_DECODE_TABLE[spriteBase[64 + 1]]
        | (SPR_DECODE_TABLE[spriteBase[64 + 0]] << 1)
        | (SPR_DECODE_TABLE[spriteBase[64 + 3]] << 2)
        | (SPR_DECODE_TABLE[spriteBase[64 + 2]] << 3);

    uint32_t pixelDataB = SPR_DECODE_TABLE[spriteBase[0 + 1]]
        | (SPR_DECODE_TABLE[spriteBase[0 + 0]] << 1)
        | (SPR_DECODE_TABLE[spriteBase[0 + 3]] << 2)
        | (SPR_DECODE_TABLE[spriteBase[0 + 2]] << 3);

    if (clippedDrawing)
    {
        drawSpriteLineClipped(
            zoomX,
            increment,
            pixelData,
            pixelDataB,
            paletteBase,
            frameBufferPtr,
            frameBuffer + ((scanline - 16) * FRAMEBUFFER_WIDTH),
            frameBuffer + ((scanline - 15) * FRAMEBUFFER_WIDTH));
    }
    else
        drawSpriteLine(zoomX, increment, pixelData, pixelDataB, paletteBase, frameBufferPtr);
}

void Video::drawBlackLine(uint32_t scanline)
{
    std::memset(&frameBuffer[(scanline - 16) * Video::FRAMEBUFFER_WIDTH], 0, Video::FRAMEBUFFER_WIDTH * sizeof(uint16_t));
}

void Video::drawEmptyLine(uint32_t scanline)
{
    uint16_t* ptr = &frameBuffer[(scanline - 16) * Video::FRAMEBUFFER_WIDTH];
    uint16_t* ptrEnd = ptr + Video::FRAMEBUFFER_WIDTH;
    uint16_t color = paletteRamPc[(activePaletteBank * 0x1000) + 4095];

    std::fill(ptr, ptrEnd, color);
}

DataPacker& operator<<(DataPacker& out, const Video& video)
{
    out << video.activePaletteBank;
    out << video.autoAnimationCounter;
    out << video.autoAnimationSpeed;
    out << video.autoAnimationFrameCounter;
    out << video.autoAnimationDisabled;
    out << video.sprDisable;
    out << video.fixDisable;
    out << video.videoEnable;
    out << video.hirqControl;
    out << video.hirqRegister;
    out << video.videoramOffset;
    out << video.videoramModulo;
    out << video.videoramData;
    out << video.sprite_x;
    out << video.sprite_y;
    out << video.sprite_zoomX;
    out << video.sprite_zoomY;
    out << video.sprite_clipping;

    return out;
}

DataPacker& operator>>(DataPacker& in, Video& video)
{
    in >> video.activePaletteBank;
    in >> video.autoAnimationCounter;
    in >> video.autoAnimationSpeed;
    in >> video.autoAnimationFrameCounter;
    in >> video.autoAnimationDisabled;
    in >> video.sprDisable;
    in >> video.fixDisable;
    in >> video.videoEnable;
    in >> video.hirqControl;
    in >> video.hirqRegister;
    in >> video.videoramOffset;
    in >> video.videoramModulo;
    in >> video.videoramData;
    in >> video.sprite_x;
    in >> video.sprite_y;
    in >> video.sprite_zoomX;
    in >> video.sprite_zoomY;
    in >> video.sprite_clipping;

    return in;
}

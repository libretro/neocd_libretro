#ifndef VIDEO_H
#define VIDEO_H

#include "datapacker.h"

#include <cstdint>

class Video
{
public:
    static constexpr uint32_t FRAMEBUFFER_WIDTH = 320;
    static constexpr uint32_t FRAMEBUFFER_HEIGHT = 224;
    static constexpr float ASPECT_RATIO = static_cast<float>(FRAMEBUFFER_WIDTH) / static_cast<float>(FRAMEBUFFER_HEIGHT); //4.0f / 3.0f;
    static constexpr uint16_t MAX_SPRITES_PER_SCREEN = 381;
    static constexpr uint16_t MAX_SPRITES_PER_LINE = 96;

    // Used to clip sprites
    static constexpr uint32_t LEFT_BORDER = 160 - (FRAMEBUFFER_WIDTH / 2);
    static constexpr uint32_t RIGHT_BORDER = (FRAMEBUFFER_WIDTH / 2) + 159;

    enum HirqControl
    {
        HIRQ_CTRL_DISABLE     = 0x00,
        HIRQ_CTRL_ENABLE      = 0x10,
        HIRQ_CTRL_RELATIVE    = 0x20,
        HIRQ_CTRL_VBLANK_LOAD = 0x40,
        HIRQ_CTRL_AUTOREPEAT  = 0x80
    };

    Video();
    ~Video();
    
    // Non copyable
    Video(const Video&) = delete;

    // Non copyable
    Video& operator=(const Video&) = delete;

    void reset();

    void convertColor(uint32_t index);
    void convertPalette();

    void updateFixUsageMap();

    void drawFix(uint32_t scanline);

    uint16_t createSpriteList(uint32_t scanline, uint16_t *spriteList) const;
    void drawSprites(uint32_t scanline, uint16_t *spriteList, uint16_t spritesToDraw);
    void drawSprite(uint32_t spriteNumber,
                    uint32_t x,
                    uint32_t y,
                    uint32_t zoomX,
                    uint32_t zoomY,
                    uint32_t scanline,
                    uint32_t clipping);
    void drawBlackLine(uint32_t scanline);
    void drawEmptyLine(uint32_t scanline);

    friend DataPacker& operator<<(DataPacker& out, const Video& video);
    friend DataPacker& operator>>(DataPacker& in, Video& video);

    uint16_t* paletteRamPc;
    uint8_t* fixUsageMap;
    uint16_t* frameBuffer;

    // Variables to save in savestate
    uint32_t activePaletteBank;
    uint32_t autoAnimationCounter;
    uint32_t autoAnimationSpeed;
    uint32_t autoAnimationFrameCounter;
    bool     autoAnimationDisabled;
    bool     sprDisable;
    bool     fixDisable;
    bool     videoEnable;
    uint32_t hirqControl;
    uint32_t hirqRegister;
    uint32_t videoramOffset;
    uint32_t videoramModulo;
    uint32_t videoramData;
    uint32_t sprite_x;
    uint32_t sprite_y;
    uint32_t sprite_zoomX;
    uint32_t sprite_zoomY;
    uint32_t sprite_clipping;
    // End variables to save in savestate
};

DataPacker& operator<<(DataPacker& out, const Video& video);
DataPacker& operator>>(DataPacker& in, Video& video);

#endif // VIDEO_H

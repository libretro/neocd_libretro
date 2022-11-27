#include "input.h"
#include "libretro_common.h"
#include "libretro_input.h"
#include "libretro.h"
#include "neogeocd.h"
#include "timeprofiler.h"

// Definition of the Neo Geo arcade stick
static const struct retro_input_descriptor neogeoCDPadDescriptors[] = {
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "A" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "B" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "C" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "D" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "B+C" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "A+B" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "C+D" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "A+B+C" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "B+C+D" },
    { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Left Analog X" },
    { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Left Analog Y" },

    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "A" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "B" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "C" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "D" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "B+C" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "A+B" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "C+D" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "A+B+C" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "B+C+D" },
    { 1, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Left Analog X" },
    { 1, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Left Analog Y" },

    { 0 }
};

// Input mapping definition
static const uint8_t padMap[] = {
    RETRO_DEVICE_ID_JOYPAD_LEFT, Input::Left,
    RETRO_DEVICE_ID_JOYPAD_UP, Input::Up,
    RETRO_DEVICE_ID_JOYPAD_DOWN, Input::Down,
    RETRO_DEVICE_ID_JOYPAD_RIGHT, Input::Right,
    RETRO_DEVICE_ID_JOYPAD_B, Input::A,
    RETRO_DEVICE_ID_JOYPAD_A, Input::B,
    RETRO_DEVICE_ID_JOYPAD_Y, Input::C,
    RETRO_DEVICE_ID_JOYPAD_X, Input::D,
    RETRO_DEVICE_ID_JOYPAD_L, Input::B | Input::C,
    RETRO_DEVICE_ID_JOYPAD_L2, Input::A | Input::B,
    RETRO_DEVICE_ID_JOYPAD_L3, Input::C | Input::D,
    RETRO_DEVICE_ID_JOYPAD_R2, Input::A | Input::B | Input::C,
    RETRO_DEVICE_ID_JOYPAD_R3, Input::B | Input::C | Input::D
};

// Input mapping definition
static const uint8_t padMap2[] = {
    0, RETRO_DEVICE_ID_JOYPAD_START, Input::Controller1Start,
    0, RETRO_DEVICE_ID_JOYPAD_SELECT, Input::Controller1Select,
    1, RETRO_DEVICE_ID_JOYPAD_START, Input::Controller2Start,
    1, RETRO_DEVICE_ID_JOYPAD_SELECT, Input::Controller2Select
};

static uint8_t readAnalogStick(unsigned port)
{
    uint8_t result = 0;
    const int16_t DEADZONE = 10000;

    const auto xAxis = libretro.inputState(port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
    if (xAxis < -DEADZONE)
        result |= Input::Left;
    if (xAxis > DEADZONE)
        result |= Input::Right;

    const auto yAxis = libretro.inputState(port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
    if (yAxis < -DEADZONE)
        result |= Input::Up;
    if (yAxis > DEADZONE)
        result |= Input::Down;

    return result;
}

void Libretro::Input::init()
{
    libretro.environment(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, (void*)neogeoCDPadDescriptors);
}

void Libretro::Input::update()
{
    uint8_t input1 = 0xFF;
    uint8_t input2 = 0xFF;
    uint8_t input3 = 0x0F;

    PROFILE(p_polling, ProfilingCategory::InputPolling);
    libretro.inputPoll();
    PROFILE_END(p_polling);

    for (size_t i = 0; i < sizeof(padMap); i += 2)
    {
        if (libretro.inputState(0, RETRO_DEVICE_JOYPAD, 0, padMap[i]))
            input1 &= ~padMap[i + 1];
    }

    input1 &= ~readAnalogStick(0);

    for (size_t i = 0; i < sizeof(padMap); i += 2)
    {
        if (libretro.inputState(1, RETRO_DEVICE_JOYPAD, 0, padMap[i]))
            input2 &= ~padMap[i + 1];
    }

    input2 &= ~readAnalogStick(1);

    for (size_t i = 0; i < sizeof(padMap2); i += 3)
    {
        if (libretro.inputState(padMap2[i], RETRO_DEVICE_JOYPAD, 0, padMap2[i + 1]))
            input3 &= ~padMap2[i + 2];
    }

    neocd->input.setInput(input1, input2, input3);
}

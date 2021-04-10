#include "memory_input.h"
#include "neogeocd.h"

static uint32_t controller1ReadByte(uint32_t address)
{
    if (!(address & 1))
    {
        switch (neocd.input.selector)
        {
        case 0x00:
        case 0x12:
        case 0x1B:
            return neocd.input.input1;
        }
    }

    return 0xFF;
}

static uint32_t controller1ReadWord(uint32_t address)
{
    switch (neocd.input.selector)
    {
    case 0x00:
    case 0x12:
    case 0x1B:
        return ((neocd.input.input1 << 8) | 0xFF);
    }

    return 0xFFFF;
}

static void controller1WriteByte(uint32_t address, uint32_t data)
{
    if (address & 1)
        neocd.timers.timer<TimerGroup::Watchdog>().setDelay(Timer::WATCHDOG_DELAY);
}

static void controller1WriteWord(uint32_t address, uint32_t data)
{
    neocd.timers.timer<TimerGroup::Watchdog>().setDelay(Timer::WATCHDOG_DELAY);
}

const Memory::Handlers controller1Handlers = {
    controller1ReadByte,
    controller1ReadWord,
    controller1WriteByte,
    controller1WriteWord
};

static uint32_t controller2ReadByte(uint32_t address)
{
    if (!(address & 1))
    {
        switch (neocd.input.selector)
        {
        case 0x00:
        case 0x12:
        case 0x1B:
            return neocd.input.input2;
        }
    }

    return 0xFF;
}

static uint32_t controller2ReadWord(uint32_t address)
{
    switch (neocd.input.selector)
    {
    case 0x00:
    case 0x12:
    case 0x1B:
        return ((neocd.input.input2 << 8) | 0xFF);
    }

    return 0xFFFF;
}

static void controller2WriteByte(uint32_t address, uint32_t data)
{
    // Nothing happens
}

static void controller2WriteWord(uint32_t address, uint32_t data)
{
    // Nothing happens
}

const Memory::Handlers controller2Handlers = {
    controller2ReadByte,
    controller2ReadWord,
    controller2WriteByte,
    controller2WriteWord
};

static uint32_t controller3ReadByte(uint32_t address)
{
    if (!(address & 1))
    {
        switch (neocd.input.selector)
        {
        case 0x00:
        case 0x12:
        case 0x1B:
            return neocd.input.input3;
        }

        return 0x0F;
    }

    return 0xFF;
}

static uint32_t controller3ReadWord(uint32_t address)
{
    switch (neocd.input.selector)
    {
    case 0x00:
    case 0x12:
    case 0x1B:
        return ((neocd.input.input3 << 8) | 0xFF);
    }

    return 0x0FFF;
}

static void controller3WriteByte(uint32_t address, uint32_t data)
{
    if (address & 0x01)
        neocd.input.selector = data & 0xFF;
}

static void controller3WriteWord(uint32_t address, uint32_t data)
{
    neocd.input.selector = data & 0xFF;
}

const Memory::Handlers controller3Handlers = {
    controller3ReadByte,
    controller3ReadWord,
    controller3WriteByte,
    controller3WriteWord
};

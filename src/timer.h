#ifndef TIMER_H
#define TIMER_H

#include "datapacker.h"
#include "round.h"

#include <cstdint>

class Timer
{
public:
    static constexpr double MASTER_CLOCK = 24000000.0; //24167828.0;
    static constexpr double M68K_CLOCK = 12000000.0; //12083914.0;
    static constexpr double Z80_CLOCK = 4000000.0; //4027971.0;
    static constexpr double PIXEL_CLOCK = 6000000.0; //6041957.0;
    static constexpr int32_t SCREEN_WIDTH = 384;
    static constexpr int32_t SCREEN_HEIGHT = 264;
    static constexpr int32_t FIRST_LINE = 16;
    static constexpr int32_t VBLANK_LINE = FIRST_LINE + 224; // 240
    static constexpr int32_t HS_START = 30;
    static constexpr int32_t WATCHDOG_DELAY = round<int32_t>(MASTER_CLOCK * 0.13516792);
    static constexpr int32_t CDROM_DELAY = round<int32_t>(MASTER_CLOCK / 75.0);
    static constexpr int32_t CYCLES_PER_FRAME = round<int32_t>((MASTER_CLOCK / PIXEL_CLOCK) * SCREEN_WIDTH * SCREEN_HEIGHT);
    static constexpr double FRAME_RATE = /*PIXEL_CLOCK*/ 6041957.0 / static_cast<double>(SCREEN_WIDTH * SCREEN_HEIGHT);

    typedef void(*Callback)(Timer* timer, uint32_t userData);

    enum State
    {
        Stopped = 0,
        Active
    };

    Timer();
    
    bool isActive() const;
    Timer::State state() const;
    void setState(const Timer::State &state);

    Timer::Callback callback() const;
    void setCallback(const Timer::Callback &callback);

    int32_t delay() const;
    void setDelay(int32_t delay);
    void arm(const int32_t delay);
    void armRelative(const int32_t delay);
    void advanceTime(const int32_t time);

    uint32_t userData() const;
    void setUserData(const uint32_t &userData);

    static inline constexpr int32_t secondsToMaster(double value)
    {
        return round<int32_t>(value * MASTER_CLOCK);
    }

    static inline constexpr double masterToSeconds(int32_t value)
    {
        return static_cast<double>(value) / MASTER_CLOCK;
    }

    static inline constexpr int32_t m68kToMaster(int32_t value)
    {
       return round<int32_t>(static_cast<double>(value) * (MASTER_CLOCK / M68K_CLOCK));
    }

    static inline constexpr int32_t z80ToMaster(int32_t value)
    {
        return round<int32_t>(static_cast<double>(value) * (MASTER_CLOCK / Z80_CLOCK));
    }

    static inline constexpr int32_t pixelToMaster(int32_t value)
    {
        return round<int32_t>(static_cast<double>(value) * (MASTER_CLOCK / PIXEL_CLOCK));
    }

    static inline constexpr int32_t masterToM68k(int32_t value)
    {
        return round<int32_t>(static_cast<double>(value) / (MASTER_CLOCK / M68K_CLOCK));
    }

    static inline constexpr int32_t masterToZ80(int32_t value)
    {
        return round<int32_t>(static_cast<double>(value) / (MASTER_CLOCK / Z80_CLOCK));
    }

    static inline constexpr int32_t masterToPixel(int32_t value)
    {
        return round<int32_t>(static_cast<double>(value) / (MASTER_CLOCK / PIXEL_CLOCK));
    }

    friend DataPacker& operator<<(DataPacker& out, const Timer& timer);
    friend DataPacker& operator>>(DataPacker& in, Timer& timer);

protected:
    void checkTimeout();

    Timer::State m_state;
    Timer::Callback m_callback;
    int32_t m_delay;
    uint32_t m_userData;
};

DataPacker& operator<<(DataPacker& out, const Timer& timer);
DataPacker& operator>>(DataPacker& in, Timer& timer);

#endif // TIMER_H

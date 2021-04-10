#ifndef TIMERGROUP_H
#define TIMERGROUP_H

#include <cstdint>
#include <array>
#include <tuple>

#include "timer.h"

class TimerGroup
{
public:
    enum Timers
    {
        Watchdog,
        Vbl,
        Hbl,
        VblReload,
        Drawline,
        Cdrom64Hz,
        Cdrom75Hz,
        Ym2610A,
        Ym2610B,
        AudioCommand,
        TimerCount
    };

    explicit TimerGroup();

    // Non copyable
    TimerGroup(const TimerGroup&) = delete;

    // Non copyable
    TimerGroup& operator=(const TimerGroup&) = delete;

    void reset();

    int32_t timeSlice() const;

    void advanceTime(const int32_t time);

    template<size_t N>
    const Timer& timer() const
    {
        return std::get<N>(m_timers);
    }

    template<size_t N>
    Timer& timer()
    {
        return std::get<N>(m_timers);
    }

    friend DataPacker& operator<<(DataPacker& out, const TimerGroup& timerGroup);
    friend DataPacker& operator>>(DataPacker& in, TimerGroup& timerGroup);

protected:
    std::array<Timer, TimerGroup::TimerCount> m_timers;
};

DataPacker& operator<<(DataPacker& out, const TimerGroup& timerGroup);
DataPacker& operator>>(DataPacker& in, TimerGroup& timerGroup);

#endif // TIMERGROUP_H

#ifndef TIMERGROUP_H
#define TIMERGROUP_H

#include "timer.h"

#include <cstdint>
#include <vector>

class TimerGroup
{
public:
    TimerGroup();

    // Non copyable
    TimerGroup(const TimerGroup&) = delete;

    // Non copyable
    TimerGroup& operator=(const TimerGroup&) = delete;

    void reset();

    int32_t timeSlice() const;

    void advanceTime(const int32_t time);

    Timer*  watchdogTimer;
    Timer*  vblTimer;
    Timer*  hirqTimer;
    Timer*  drawlineTimer;
    Timer*  cdromIRQTimer;
    Timer*  ym2610TimerA;
    Timer*  ym2610TimerB;
    Timer*  audioCommandTimer;

    friend DataPacker& operator<<(DataPacker& out, const TimerGroup& timerGroup);
    friend DataPacker& operator>>(DataPacker& in, TimerGroup& timerGroup);

protected:
    std::vector<Timer> m_timers;
};

DataPacker& operator<<(DataPacker& out, const TimerGroup& timerGroup);
DataPacker& operator>>(DataPacker& in, TimerGroup& timerGroup);

#endif // TIMERGROUP_H

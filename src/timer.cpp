#include "timer.h"

/*
							drawlineTimer
									^
									|
								0	30			  350 384
							0	+-----------------------+
								|#######################|
							16	|###+---------------+###|
								|###|	   320		|###|
								|###|				|###|
								|###| 224			|###|
								|###|				|###|
								|###|				|###|
		vblTimer ->			240 |###+---------------+###|
								|#######################|
							264 +-----------------------+

	Pixel clock = 6000000 Hz
	Lines per second = 6000000 / 384 = 15625
	Frames per second = 15625 / 264 (approx 59.1856)

	MC68000 clock = 12000000
	Cycles per pixel = 2
	Cycles per line = 768
	Cycles per screen = 202752

	Z80 clock = 4000000
	Cycles per pixel = 4/6 (approx 0.6666)
	Cycles per line = 256
	Cycles per screen = 67584
*/

Timer::Timer() :
    m_state(Timer::Stopped),
    m_callback(nullptr),
    m_delay(0),
    m_userData(0)
{
}

bool Timer::isActive() const
{
    return (m_state == Timer::Active);
}

Timer::State Timer::state() const
{
    return m_state;
}

void Timer::setState(const Timer::State &state)
{
    m_state = state;
    checkTimeout();
}

Timer::Callback Timer::callback() const
{
    return m_callback;
}

void Timer::setCallback(const Timer::Callback &callback)
{
    m_callback = callback;
}

int32_t Timer::delay() const
{
    return m_delay;
}

void Timer::setDelay(int32_t delay)
{
	m_delay = delay;
}

void Timer::arm(const int32_t delay)
{
    m_delay = delay;
    m_state = Timer::Active;
    checkTimeout();
}

void Timer::armRelative(const int32_t delay)
{
    m_delay += delay;
    m_state = Timer::Active;
    checkTimeout();
}

void Timer::advanceTime(const int32_t time)
{
    if (!isActive())
        return;

    m_delay -= time;
    checkTimeout();
}

uint32_t Timer::userData() const
{
    return m_userData;
}

void Timer::setUserData(const uint32_t &userData)
{
    m_userData = userData;
}

void Timer::checkTimeout()
{
    if (!isActive() || (m_delay > 0))
        return;

    m_state = Timer::Stopped;

    if (m_callback)
        m_callback(this, m_userData);
}

DataPacker& operator<<(DataPacker& out, const Timer& timer)
{
	out << timer.m_state;
	out << timer.m_delay;
	out << timer.m_userData;

	return out;
}

DataPacker& operator>>(DataPacker& in, Timer& timer)
{
	in >> timer.m_state;
	in >> timer.m_delay;
	in >> timer.m_userData;

	return in;
}

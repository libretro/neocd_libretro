#include "timeprofiler.h"

#ifdef PROFILE_ENABLED

#include <cstring>

bool	g_profilingAccumulatorsInitialized = false;
double		g_profilingAccumulators[CategoryCount];

TimeProfiler::TimeProfiler(ProfilingCategory category) : m_category(category)
{
    if (!g_profilingAccumulatorsInitialized)
    {
        std::memset(g_profilingAccumulators, 0, sizeof(g_profilingAccumulators));
        g_profilingAccumulatorsInitialized = true;
    }

    start();
}

TimeProfiler::~TimeProfiler()
{
    end();
}

void TimeProfiler::start()
{
    m_start = std::chrono::high_resolution_clock::now();
    m_running = true;
}

void TimeProfiler::end()
{
    if (!m_running)
        return;

    std::chrono::time_point<std::chrono::high_resolution_clock> end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> elapsed = end - m_start;

    g_profilingAccumulators[m_category] += elapsed.count();

    m_running = false;
}

#endif // PROFILE_ENABLED

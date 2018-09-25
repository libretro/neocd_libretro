#ifndef TIMEPROFILER_H
#define TIMEPROFILER_H

//#define PROFILE_ENABLED

enum ProfilingCategory
{
    Total = 0,
    AudioCD,
    AudioYM2610,
    CpuM68K,
    CpuZ80,
    VideoAndIRQ,
    InputPolling,
    CategoryCount
};

#ifdef PROFILE_ENABLED

#include <chrono>

extern bool     g_profilingAccumulatorsInitialized;
extern double   g_profilingAccumulators[CategoryCount];

class TimeProfiler
{
public:
    explicit TimeProfiler(ProfilingCategory category);
    virtual ~TimeProfiler();

    void start();
    void end();

protected:
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
    bool                m_running;
    ProfilingCategory   m_category;
};

#define PROFILE(ObjectName, Category) TimeProfiler ObjectName(Category)
#define PROFILE_START(ObjectName) ObjectName.start()
#define PROFILE_END(ObjectName) ObjectName.end()

#else

#define PROFILE(ObjectName, Category)
#define PROFILE_START(ObjectName)
#define PROFILE_END(ObjectName)

#endif // PROFILE_ENABLED

#endif // TIMEPROFILER_H

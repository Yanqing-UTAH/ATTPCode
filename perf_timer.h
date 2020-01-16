#ifndef PERF_TIMER_H
#define PERF_TIMER_H

#include <chrono>
#include <cstdint>

using std::uint64_t;

class PerfTimer
{
private:
    typedef std::chrono::high_resolution_clock clock;

public:
    PerfTimer();

    ~PerfTimer();

    void
    measure_start();

    void
    measure_end();

    uint64_t
    get_elapsed_ms() const;

    uint64_t
    get_elapsed_s() const;

    uint64_t
    get_avg_elapsed_us() const;

    uint64_t
    get_avg_elapsed_ms() const;

    uint64_t
    get_avg_elapsed_s() const;

private:
    clock::duration             m_elapsed;

    clock::time_point           m_last_start;

    uint64_t                    m_num_calls;
};


#endif // PERF_TIMER_H


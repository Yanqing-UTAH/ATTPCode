#include "perf_timer.h"

PerfTimer::PerfTimer():
    m_elapsed(clock::duration::zero()),
    m_last_start(),
    m_num_calls(0)
{}

PerfTimer::~PerfTimer()
{}

void
PerfTimer::measure_start()
{
    m_last_start = clock::now();
}

void
PerfTimer::measure_end()
{
    auto end = clock::now();
    m_elapsed += end - m_last_start;
    ++m_num_calls;
}

uint64_t
PerfTimer::get_elapsed_ms() const
{
    return (uint64_t) std::chrono::duration_cast<std::chrono::milliseconds>(m_elapsed).count();
}

uint64_t
PerfTimer::get_elapsed_s() const
{
    return (uint64_t) std::chrono::duration_cast<std::chrono::seconds>(m_elapsed).count();
}

uint64_t
PerfTimer::get_avg_elapsed_us() const
{
    if (!m_num_calls) return 0;
    return (uint64_t) std::chrono::duration_cast<std::chrono::microseconds>(
            m_elapsed / (double) m_num_calls).count();
}

uint64_t
PerfTimer::get_avg_elapsed_ms() const
{
    if (!m_num_calls) return 0;
    return (uint64_t) std::chrono::duration_cast<std::chrono::milliseconds>(
            m_elapsed / (double) m_num_calls).count();
}

uint64_t
PerfTimer::get_avg_elapsed_s() const
{
    if (!m_num_calls) return 0;
    return (uint64_t) std::chrono::duration_cast<std::chrono::seconds>(
            m_elapsed / (double) m_num_calls).count();
}


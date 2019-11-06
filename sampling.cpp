#include "sampling.h"
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <cstdlib>
#include "util.h"

SamplingSketch::List::List():
    m_length(0u),
    m_capacity(0u),
    m_items(nullptr),
    m_end_of_content(0u),
    m_capacity_of_content(0u),
    m_content(nullptr)
{
}

SamplingSketch::List::~List()
{
    reset();
}

void
SamplingSketch::List::reset()
{
    delete[] m_items;
    m_length = m_capacity = 0;
    delete[] m_content;
    m_end_of_content = m_capacity_of_content = 0;
}

void
SamplingSketch::List::append(
    unsigned long long ts,
    const char *str)
{
    if (m_length == m_capacity)
    {
        unsigned new_capacity = (m_capacity) ? (m_capacity << 1) : 16u;
        Item *new_items = new Item[new_capacity];
        if (m_capacity)
        {
            memcpy(new_items, m_items, sizeof(Item) * m_capacity);
        }
        delete[] m_items;
        m_capacity = new_capacity;
        m_items = new_items;
    }

    size_t value_len = strlen(str);
    if (m_end_of_content + value_len >= m_capacity_of_content)
    {
        unsigned new_capacity_of_content =
            (m_capacity_of_content) ? (m_capacity_of_content << 1) : 1024u;
        while (m_end_of_content + value_len >= new_capacity_of_content)
        {
            new_capacity_of_content = new_capacity_of_content << 1;
        }
        char *new_content = new char[new_capacity_of_content];
        if (m_capacity_of_content)
        {
            memcpy(new_content, m_content, m_end_of_content);
        }
        delete[] m_content;
        m_capacity_of_content = new_capacity_of_content;
        m_content = new_content;
    }

    m_items[m_length].m_ts = ts;
    m_items[m_length].m_value_off = m_end_of_content;
    memcpy(m_content + m_end_of_content, str, value_len + 1);
    ++m_length;
    m_end_of_content += value_len + 1;
}

size_t
SamplingSketch::List::memory_usage() const
{
    return m_capacity * sizeof(Item) + m_capacity_of_content;
}

const char *
SamplingSketch::List::last_of(unsigned long long ts) const
{
    return m_content + (std::upper_bound(m_items, m_items + m_length, ts,
        [](unsigned long long ts, const Item& i) -> bool {
            return ts < i.m_ts;
        }) - 1)->m_value_off;
}

SamplingSketch::SamplingSketch(
    unsigned sample_size,
    unsigned seed):
    m_sample_size(sample_size),
    m_seen(0ull),
    m_reservoir(new List[sample_size]),
    m_rng(seed)
{
}

SamplingSketch::~SamplingSketch()
{
    delete[] m_reservoir;
}

void
SamplingSketch::update(unsigned long long ts, const char *str, int c) {
update_loop:
    if (m_seen < m_sample_size)
    {
        m_reservoir[m_seen++].append(ts, str);
    }
    else
    {
        std::uniform_int_distribution<unsigned long long> unif(0, m_seen++);
        auto i = unif(m_rng);
        if (i < m_sample_size)
        {
            m_reservoir[i].append(ts, str);
        }
    }

    if (--c) goto update_loop;
}

void
SamplingSketch::clear()
{
    for (unsigned i = 0; i < m_sample_size; ++i)
    {
        m_reservoir[i].reset();
    }
    m_seen = 0;
}

size_t
SamplingSketch::memory_usage()
{
    size_t sum = sizeof(*this);
    for (unsigned i = 0; i < m_sample_size; ++i)
    {
        sum += m_reservoir[i].memory_usage();
    }
    return sum;
}

double
SamplingSketch::estimate_point_at_the_time(
    const char *str,
    unsigned long long ts_e)
{
    unsigned n = (unsigned) std::min((unsigned long long) m_sample_size, m_seen);
    unsigned c = 0;
    for (unsigned i = 0; i < n; ++i)
    {
        const char *s = m_reservoir[i].last_of(ts_e);
        if (!strcmp(s, str))
        {
            ++c;
        }
    }
    
    if (m_seen <= m_sample_size)
    {
        return (double) c;
    }
    else
    {
        return c * ((double) m_seen / m_sample_size);
    }
}

SamplingSketch*
SamplingSketch::create(
    int argc,
    char *argv[],
    const char **help_str)
{
    if (argc < 1)
    {
        if (help_str) *help_str = " <sample size> [seed]";
        return nullptr;
    }

    char *str_end;
    auto v = strtol(argv[0], &str_end, 0);
    if (!check_long_ii(v, 1, MAX_SAMPLE_SIZE, str_end))
    {
        if (help_str) *help_str = " <sample size> [seed]\n[Error] Invalid sample size";
        return nullptr;
    }
    unsigned sample_size = (unsigned) v;

    if (argc < 2)
    {
        return new SamplingSketch(sample_size);
    }
    
    v = strtol(argv[0], &str_end, 0);
    if (!check_long_ii(v, 0, ~0u, str_end))
    {
        if (help_str) *help_str = " <sample size> [seed]\n[Error] Invalid seed";
        return nullptr;
    }
    unsigned seed = (unsigned) v;

    return new SamplingSketch(sample_size, seed);
}


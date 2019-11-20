#include "sampling.h"
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <cstdlib>
#include <unordered_map>
#include "util.h"
#include "conf.h"
#include <sstream>

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
    ensure_item_capacity(m_length + 1);

    size_t value_len = strlen(str);
    ensure_content_capacity(m_end_of_content + value_len + 1);

    m_items[m_length].m_ts = ts;
    m_items[m_length].m_value.m_str_off = m_end_of_content;
    memcpy(m_content + m_end_of_content, str, value_len + 1);
    ++m_length;
    m_end_of_content += value_len + 1;
}

void
SamplingSketch::List::append(
    TIMESTAMP ts,
    uint32_t value)
{
    ensure_item_capacity(m_length + 1); 

    m_items[m_length].m_ts = ts;
    m_items[m_length++].m_value.m_u32 = value;
}

size_t
SamplingSketch::List::memory_usage() const
{
    return m_capacity * sizeof(Item) + m_capacity_of_content;
}

SamplingSketch::Item*
SamplingSketch::List::last_of(unsigned long long ts) const
{
    Item * item = std::upper_bound(m_items, m_items + m_length, ts,
        [](unsigned long long ts, const Item& i) -> bool {
            return ts < i.m_ts;
        });
    return (item == m_items) ? nullptr : (item - 1);
}

void
SamplingSketch::List::ensure_item_capacity(unsigned desired_length)
{
    if (desired_length > m_capacity)
    {
        unsigned new_capacity = (m_capacity) ? (m_capacity << 1) : 16u;
        while (desired_length > new_capacity)
        {
            new_capacity = new_capacity << 1;
        }
        Item *new_items = new Item[new_capacity];
        if (m_capacity)
        {
            memcpy(new_items, m_items, sizeof(Item) * m_capacity);
        }
        delete[] m_items;
        m_capacity = new_capacity;
        m_items = new_items;
    }
}

void
SamplingSketch::List::ensure_content_capacity(size_t desired_length)
{
    if (desired_length > m_capacity_of_content)
    {
        unsigned new_capacity_of_content =
            (m_capacity_of_content) ? (m_capacity_of_content << 1) : 1024u;
        while (desired_length > new_capacity_of_content)
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
SamplingSketch::update(unsigned long long ts, const char *str, int c)
{
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
SamplingSketch::update(TIMESTAMP ts, uint32_t value, int c)
{
update_loop:
    if (m_seen < m_sample_size)
    {
        m_reservoir[m_seen++].append(ts, value);
    }
    else
    {
        std::uniform_int_distribution<unsigned long long> unif(0, m_seen++);
        auto i = unif(m_rng);
        if (i < m_sample_size)
        {
            m_reservoir[i].append(ts, value);
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
SamplingSketch::memory_usage() const
{
    size_t sum = sizeof(*this);
    for (unsigned i = 0; i < m_sample_size; ++i)
    {
        sum += m_reservoir[i].memory_usage();
    }
    return sum;
}

std::string
SamplingSketch::get_short_description() const
{
    std::ostringstream oss;
    oss << "SAMPLING-ss" << m_sample_size;
    return oss.str();
}

double
SamplingSketch::estimate_point_at_the_time(
    const char *str,
    unsigned long long ts_e)
{
    unsigned n = (unsigned) std::min((unsigned long long) m_sample_size, m_seen);
    unsigned nvalid = 0;
    unsigned c = 0;
    for (unsigned i = 0; i < n; ++i)
    {
        Item *item = m_reservoir[i].last_of(ts_e);
        if (item)
        {
            const char *s = m_reservoir[i].get_str(*item);
            if (!strcmp(s, str))
            {
                ++c;
            }
            ++nvalid;
        }
    }
    
    if (nvalid <= m_sample_size)
    {
        return (double) c;
    }
    else
    {
        return c * ((double) m_seen / m_sample_size);
    }
}

std::vector<IPersistentHeavyHitterSketch::HeavyHitter>
SamplingSketch::estimate_heavy_hitters(
    TIMESTAMP ts_e,
    double frac_threshold) const
{
    std::unordered_map<uint32_t, unsigned> cnt_map;
    
    unsigned n = (unsigned) std::min((unsigned long long) m_sample_size, m_seen);
    unsigned nvalid = 0;
    for (unsigned i = 0; i < n; ++i)
    {
        Item *item = m_reservoir[i].last_of(ts_e);
        if (item)
        {
            ++nvalid;
            ++cnt_map[item->m_value.m_u32];
        }
    }

    std::vector<IPersistentHeavyHitterSketch::HeavyHitter> ret;
    auto sample_threshold = (unsigned) std::ceil(frac_threshold * nvalid);
    for (const auto &p: cnt_map)
    {
        if (p.second >= sample_threshold)
        {
            ret.emplace_back(IPersistentHeavyHitterSketch::HeavyHitter{
                    p.first, (float) p.second / nvalid});
        }
    }
    return std::move(ret);
}

SamplingSketch*
SamplingSketch::create(
    int &argi,
    int argc,
    char *argv[],
    const char **help_str)
{
    if (argi >= argc)
    {
        if (help_str) *help_str = " <sample size> [seed]\n";
        return nullptr;
    }

    char *str_end;
    auto v = strtol(argv[argi++], &str_end, 0);
    if (!check_long_ii(v, 1, MAX_SAMPLE_SIZE, str_end))
    {
        if (help_str) *help_str = " <sample size> [seed]\n[Error] Invalid sample size\n";
        return nullptr;
    }
    unsigned sample_size = (unsigned) v;

    if (argi >= argc)
    {
        return new SamplingSketch(sample_size);
    }
    
    v = strtol(argv[argi++], &str_end, 0);
    if (!check_long_ii(v, 0, ~0u, str_end))
    {
        if (help_str) *help_str = " <sample size> [seed]\n[Error] Invalid seed\n";
        return nullptr;
    }
    unsigned seed = (unsigned) v;

    return new SamplingSketch(sample_size, seed);
}

SamplingSketch*
SamplingSketch::get_test_instance()
{
    return new SamplingSketch(1);
}

SamplingSketch*
SamplingSketch::create_from_config(
    int idx)
{
    uint32_t sample_size, seed;

    sample_size = g_config->get_u32("SAMPLING.sample_size", idx).value();
    seed = g_config->get_u32("SAMPLING.seed", -1).value();

    return new SamplingSketch(sample_size, seed);
}

int
SamplingSketch::num_configs_defined()
{
    if (g_config->is_list("SAMPLING.sample_size"))
    {
        return g_config->list_length("SAMPLING.sample_size");   
    }

    return -1;
}


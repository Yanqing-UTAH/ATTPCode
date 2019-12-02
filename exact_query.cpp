#include "exact_query.h"
#include <algorithm>
#include <numeric>
#include <sstream>
#include "conf.h"

ExactHeavyHitters::ExactHeavyHitters():
    m_items(),
    m_initial_bucket_count(m_items.bucket_count())
{
}

ExactHeavyHitters::~ExactHeavyHitters()
{
}

void
ExactHeavyHitters::clear()
{
    m_items.clear();
    m_items.rehash(m_initial_bucket_count);
}


size_t
ExactHeavyHitters::memory_usage() const
{
    // assuming gcc
    return 
        8 + // initial bucket count
        size_of_unordered_map(m_items) +
        std::accumulate(m_items.cbegin(), m_items.cend(),
            (decltype(m_items.size())) 0,
            [](auto acc, const auto &p) -> auto {
                return acc + p.second.capacity() * sizeof(Item);
            }); // item arrays
}

std::string
ExactHeavyHitters::get_short_description() const
{
    return "EXACT_HH";
}

void
ExactHeavyHitters::update(
    TIMESTAMP ts,
    uint32_t value,
    int c)
{
    std::vector<Item> &item_vec = m_items[value]; 
    if (item_vec.empty())
    {
        item_vec.emplace_back(Item{ts, (uint64_t) c});
    }
    else if (item_vec.back().m_ts == ts) {
        item_vec.back().m_cnt += c;
    }
    else
    {
        item_vec.emplace_back(Item{ts, item_vec.back().m_cnt + c});
    }
}

std::vector<IPersistentHeavyHitterSketch::HeavyHitter>
ExactHeavyHitters::estimate_heavy_hitters(
    TIMESTAMP ts_e,
    double frac_threshold) const
{
    std::vector<std::pair<uint32_t, uint64_t>> snapshot;

    uint64_t tot_cnt = 0;
    for (const auto &p: m_items)
    {
        auto &item_vec = p.second;
        auto upper_ptr = std::upper_bound(item_vec.begin(), item_vec.end(),
            ts_e, [](TIMESTAMP ts, const Item i) -> bool { return ts < i.m_ts; });
        if (upper_ptr != item_vec.begin())
        {
            snapshot.emplace_back(p.first, upper_ptr[-1].m_cnt);
            tot_cnt += upper_ptr[-1].m_cnt;
        }
    }
    
    double threshold = frac_threshold * tot_cnt;
    std::vector<IPersistentHeavyHitterSketch::HeavyHitter> ret;
    for (const auto &item: snapshot)
    {
        if (item.second > threshold)
        {
            ret.emplace_back(IPersistentHeavyHitterSketch::HeavyHitter{
                item.first, (float) item.second / tot_cnt});
        }
    }

    return std::move(ret);
}

ExactHeavyHitters*
ExactHeavyHitters::create(
    int &argi,
    int argc,
    char *argv[],
    const char **help_str)
{
    if (*help_str) help_str = nullptr;
    return new ExactHeavyHitters();
}

ExactHeavyHitters*
ExactHeavyHitters::get_test_instance()
{
    return new ExactHeavyHitters();
}

ExactHeavyHitters*
ExactHeavyHitters::create_from_config(
    int idx)
{
    return new ExactHeavyHitters();
}

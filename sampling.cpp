#include "sampling.h"
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <cstdlib>
#include <unordered_map>
#include "util.h"
#include "conf.h"
#include <sstream>
#include "min_heap.h"

//#ifdef NDEBUG
//#undef NDEBUG
//#endif

#include <cassert>
#include <iostream>

// internal structs
namespace SamplingSketchInternals {
struct Item {
    unsigned long long      m_ts;
    union {
        // XXX the str interface is kept for the old tests
        ptrdiff_t           m_str_off;

        uint32_t            m_u32;

    }                       m_value;
};

struct List {
public:
    List();

    ~List();

    void
    reset();

    void
    append(
        TIMESTAMP ts,
        const char *str);

    void
    append(
        TIMESTAMP ts,
        uint32_t value);

    size_t
    memory_usage() const;

    Item*
    last_of(unsigned long long ts) const;

    const char*
    get_str(const Item &item) const
    {
        return m_content + item.m_value.m_str_off;
    }

private: 
    void ensure_item_capacity(unsigned desired_length);

    void ensure_content_capacity(size_t desired_length);

    // 32 bits should be enough for length which grows in logarithm
    unsigned            m_length,

                        m_capacity;

    Item                *m_items;

    size_t              m_end_of_content,

                        m_capacity_of_content;

    char                *m_content;
};

List::List():
    m_length(0u),
    m_capacity(0u),
    m_items(nullptr),
    m_end_of_content(0u),
    m_capacity_of_content(0u),
    m_content(nullptr)
{
}

List::~List()
{
    reset();
}

void
List::reset()
{
    delete[] m_items;
    m_length = m_capacity = 0;
    delete[] m_content;
    m_end_of_content = m_capacity_of_content = 0;
}

void
List::append(
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
List::append(
    TIMESTAMP ts,
    uint32_t value)
{
    ensure_item_capacity(m_length + 1); 

    m_items[m_length].m_ts = ts;
    m_items[m_length++].m_value.m_u32 = value;
}

size_t
List::memory_usage() const
{
    return m_capacity * sizeof(Item) + m_capacity_of_content;
}

Item*
List::last_of(unsigned long long ts) const
{
    Item * item = std::upper_bound(m_items, m_items + m_length, ts,
        [](unsigned long long ts, const Item& i) -> bool {
            return ts < i.m_ts;
        });
    return (item == m_items) ? nullptr : (item - 1);
}

void
List::ensure_item_capacity(unsigned desired_length)
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
List::ensure_content_capacity(size_t desired_length)
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

} // namespace SamplingSketchInternals


//////////////////////////////////////
//  Sampling sketch ATTP impl.      //
//////////////////////////////////////

using namespace SamplingSketchInternals;

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
        if (p.second > sample_threshold)
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

//////////////////////////////////////
//  Sampling sketch BITP impl.      //
//////////////////////////////////////

#define SSBITP_ITEM_OFFSET_OF(member) \
    (dsimpl::PayloadOffset) offsetof(SamplingSketchBITP::Item, member) - \
    (dsimpl::PayloadOffset) offsetof(SamplingSketchBITP::Item, m_payload)

const dsimpl::AVLNodeDesc<TIMESTAMP>
SamplingSketchBITP::m_ts_map_node_desc(
    SSBITP_ITEM_OFFSET_OF(m_ts),
    SSBITP_ITEM_OFFSET_OF(m_payload) + 0,
    SSBITP_ITEM_OFFSET_OF(m_payload) + sizeof(Item*),
    SSBITP_ITEM_OFFSET_OF(m_payload) + 2 * sizeof(Item*),
    SSBITP_ITEM_OFFSET_OF(m_payload) + 6 * sizeof(Item*));

const dsimpl::AVLNodeDesc<double>
SamplingSketchBITP::m_weight_map_node_desc(
    SSBITP_ITEM_OFFSET_OF(m_min_weight_list.m_weight),
    SSBITP_ITEM_OFFSET_OF(m_payload) + 3 * sizeof(Item*),
    SSBITP_ITEM_OFFSET_OF(m_payload) + 4 * sizeof(Item*),
    SSBITP_ITEM_OFFSET_OF(m_payload) + 5 * sizeof(Item*),
    SSBITP_ITEM_OFFSET_OF(m_payload) + 6 * sizeof(Item*) + sizeof(int));

SamplingSketchBITP::SamplingSketchBITP(
    uint32_t sample_size,
    uint32_t seed):
    m_sample_size(sample_size),
    m_ts_map(&m_ts_map_node_desc),
    m_min_weight_map(&m_weight_map_node_desc),
    m_most_recent_items_weight_map(&m_weight_map_node_desc),
    m_most_recent_items(new Item*[m_sample_size]),
    m_most_recent_items_start(0),
    m_rng(seed),
    m_unif_0_1(0.0, 1.0),
    m_num_items_alloced(0),
    m_num_mwlistnodes_alloced(0)
{
    std::memset(m_most_recent_items, 0, sizeof(Item*) * m_sample_size);
}

SamplingSketchBITP::~SamplingSketchBITP()
{
    clear();
    delete[] m_most_recent_items;
}

void
SamplingSketchBITP::clear()
{
    for (auto iter = m_ts_map.begin();
            iter != m_ts_map.end();)
    {
        Item *item = iter.get_node();
        ++iter;
        MinWeightListNode *n = item->m_min_weight_list.m_next;
        while (n)
        {
            auto n2 = n;
            n = n->m_next;
            assert(m_num_mwlistnodes_alloced);
            --m_num_mwlistnodes_alloced;
            delete n2;
        }
        assert(m_num_items_alloced);
        --m_num_items_alloced;
        delete item;
    }
    m_ts_map.clear();
    m_min_weight_map.clear();
    m_most_recent_items_weight_map.clear();
    for (uint32_t i = 0; i < m_sample_size; ++i)
    {
        if (m_most_recent_items[i])
        {
            assert(!m_most_recent_items[i]->m_min_weight_list.m_next);
            assert(m_num_items_alloced);
            --m_num_items_alloced;
            delete m_most_recent_items[i]; 
            m_most_recent_items[i] = nullptr;
        }
    }
    
    assert(m_num_items_alloced == 0);
    assert(m_num_mwlistnodes_alloced == 0);

    //m_num_items_alloced = 0;
    //m_num_mwlistnodes_alloced = 0;
}

size_t
SamplingSketchBITP::memory_usage() const
{
    std::cout << get_short_description() << ": "
        << m_num_items_alloced << ' ' << m_num_mwlistnodes_alloced << std::endl;
    return 8 // m_sample_size and alignment
        + sizeof(m_ts_map)
        + sizeof(m_min_weight_map)
        + sizeof(m_most_recent_items_weight_map)
        + 16 + m_sample_size * sizeof(Item*) // m_most_recent_items and its start
        + sizeof(m_rng)
        + sizeof(m_unif_0_1)
        + 16 // m_num_items_alloced and m_num_mwlistnodes_alloced
        + sizeof(Item) * m_num_items_alloced
        + sizeof(MinWeightListNode) * m_num_mwlistnodes_alloced;
}

std::string
SamplingSketchBITP::get_short_description() const
{
    return std::string("SAMPLING_BITP-ss") + std::to_string(m_sample_size);
}

void
SamplingSketchBITP::update(
    TIMESTAMP ts,
    uint32_t value,
    int c)
{
update_loop:
    double weight = m_unif_0_1(m_rng);

    Item *item = new Item;
    item->m_ts = ts;
    item->m_value = value;
    item->m_my_weight = weight;
    item->m_min_weight_list.m_weight = weight;
    item->m_min_weight_list.m_next = nullptr;
    ++m_num_items_alloced;
    
    bool is_first_m_sample_size_items = !ith_most_recent_item(m_sample_size - 1);
    if (!is_first_m_sample_size_items)
    {
        Item *purgeable_item = ith_most_recent_item(m_sample_size - 1);
        m_most_recent_items_weight_map.erase(purgeable_item);

        auto iter = m_most_recent_items_weight_map.begin();
        if (iter != m_most_recent_items_weight_map.end() &&
            iter->m_min_weight_list.m_weight < purgeable_item->m_min_weight_list.m_weight)
        {
            // construct the mwlist for the purgeable_item
            double original_weight = purgeable_item->m_min_weight_list.m_weight;
            purgeable_item->m_min_weight_list.m_weight = iter->m_min_weight_list.m_weight;

            MinWeightListNode **p_tail = &purgeable_item->m_min_weight_list.m_next;
            for (++iter; iter != m_most_recent_items_weight_map.end() &&
                    iter->m_min_weight_list.m_weight < original_weight; ++iter)
            {
                MinWeightListNode *p2 = new MinWeightListNode;
                p2->m_weight = iter->m_min_weight_list.m_weight;
                *p_tail = p2;
                p_tail = &p2->m_next;
                ++m_num_mwlistnodes_alloced;
            }
            
            MinWeightListNode *p2 = new MinWeightListNode;
            p2->m_weight = original_weight;
            p2->m_next = nullptr;
            *p_tail = p2;
            ++m_num_mwlistnodes_alloced;
            
            m_min_weight_map.insert(purgeable_item);
            m_ts_map.insert(purgeable_item);
        }
        else
        {
            if (purgeable_item->m_min_weight_list.m_weight < weight)
            {
                // this item is going to be purged in this update
                // don't bother add it to the min_weight_map
                delete purgeable_item;
                --m_num_items_alloced;
            }
            else
            {
                m_min_weight_map.insert(purgeable_item);
                m_ts_map.insert(purgeable_item);
            }
        }
    }

    // purge any item in the min_weight_map that
    // 1. does not have anything in the min weight list other than itself; and
    // 2. has a smaller weight than this item's weight
    //
    // or the head of the min weight list that is not the item itself
    // and has a weight smaller than this item's weight
    for (auto iter2 = m_min_weight_map.begin();
            iter2 != m_min_weight_map.end() &&
            iter2->m_min_weight_list.m_weight < weight;)
    {
        Item *purged_item = iter2.get_node();
        if (purged_item->m_min_weight_list.m_next)
        {
            MinWeightListNode *p = purged_item->m_min_weight_list.m_next;
            purged_item->m_min_weight_list.m_weight = p->m_weight;
            purged_item->m_min_weight_list.m_next = p->m_next;
            delete p;
            assert(m_num_mwlistnodes_alloced > 0);
            --m_num_mwlistnodes_alloced;

            if (weight < purged_item->m_my_weight)
            {
                if (weight < purged_item->m_min_weight_list.m_weight)
                {
                    p = new MinWeightListNode;
                    p->m_weight = purged_item->m_min_weight_list.m_weight;
                    p->m_next = purged_item->m_min_weight_list.m_next;
                    purged_item->m_min_weight_list.m_next = p;
                    purged_item->m_min_weight_list.m_weight = weight;
                }
                else
                {
                    MinWeightListNode **pp = &purged_item->m_min_weight_list.m_next;
                    while ((*pp)->m_weight < weight)
                    {
                        pp = &(*pp)->m_next;
                    }
                    p = new MinWeightListNode;
                    p->m_weight = weight;
                    p->m_next = *pp;
                    *pp = p;
                }
            }

            ++iter2;
        }
        else
        {
            iter2 = m_min_weight_map.erase(iter2);
            m_ts_map.erase(purged_item);
            assert(m_num_items_alloced > 0);
            delete purged_item;
            --m_num_items_alloced;
        }
    }

    // add this item to the most recent items
    m_most_recent_items_start = (m_most_recent_items_start == m_sample_size - 1) ?
        0 : (m_most_recent_items_start + 1);
    m_most_recent_items[m_most_recent_items_start] = item;
    m_most_recent_items_weight_map.insert(item);
    assert(!item->m_min_weight_list.m_next);

    if (--c) goto update_loop;
}

std::vector<IPersistentHeavyHitterSketchBITP::HeavyHitter>
SamplingSketchBITP::estimate_heavy_hitters_bitp(
    TIMESTAMP ts_s,
    double frac_threshold) const
{
    std::vector<Item*> samples;
    samples.reserve(m_sample_size);
    
    for (uint32_t i = 0; i < m_sample_size; ++i)
    {
        Item *item = ith_most_recent_item(i);
        if (!item || item->m_ts <= ts_s)
        {
            break;
        }
        samples.push_back(item);
    }
    
    if (samples.size() == m_sample_size)
    {
        // go through the tree
        dsimpl::min_heap_make(samples, m_sample_size,
            [](Item *item) -> double { return item->m_my_weight; });
        
        auto iter = m_ts_map.end();
        auto begin = m_ts_map.begin();
        while (iter != begin)
        {
            --iter;
            if (iter->m_ts <= ts_s)
            {
                break;
            }
            
            if (iter->m_my_weight > samples[0]->m_my_weight)
            {
                samples[0] = iter.get_node();
                dsimpl::min_heap_push_down(samples, 0, m_sample_size,
                    [](Item *item) -> double { return item->m_my_weight; });
            }
        }
    }

    std::unordered_map<uint32_t, uint32_t> cnt_map;
    for (Item *item: samples)
    {
        ++cnt_map[item->m_value];
    }
    
    std::vector<IPersistentHeavyHitterSketchBITP::HeavyHitter> ret;
    uint32_t m = (uint32_t) samples.size();
    auto sample_threshold = (uint32_t) std::ceil(frac_threshold * m);
    for (const auto &p: cnt_map)
    {
        if (p.second > sample_threshold)
        {
            ret.emplace_back(IPersistentHeavyHitterSketchBITP::HeavyHitter{
                p.first, (float) p.second / m}); 
        }
    }

    return std::move(ret);
}

SamplingSketchBITP*
SamplingSketchBITP::get_test_instance()
{
    return new SamplingSketchBITP(1);
}

SamplingSketchBITP*
SamplingSketchBITP::create_from_config(
    int idx)
{
    uint32_t sample_size, seed;

    sample_size = g_config->get_u32("SAMPLING_BITP.sample_size", idx).value();
    seed = g_config->get_u32("SAMPLING.seed", -1).value();

    return new SamplingSketchBITP(sample_size, seed);
}

int
SamplingSketchBITP::num_configs_defined()
{
    if (g_config->is_list("SAMPLING_BITP.sample_size"))
    {
        return g_config->list_length("SAMPLING_BITP.sample_size");
    }

    return -1;
}


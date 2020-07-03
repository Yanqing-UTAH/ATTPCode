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
    unsigned seed,
    bool enable_frequency_estimation):
    m_enable_frequency_estimation(enable_frequency_estimation),
    m_sample_size(sample_size),
    m_seen(0ull),
    m_reservoir(new List[sample_size]),
    m_rng(seed),
    m_last_ts(0),
    m_tmp_cnt_ts(0),
    m_tmp_cnt_map(),
    m_ts2cnt_map()
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
    if (m_enable_frequency_estimation)
    {
        if (ts == m_tmp_cnt_ts)
        {
            m_tmp_cnt_ts = 0;
            m_tmp_cnt_map.clear();
        }

        if (ts != m_last_ts)
        {
            m_ts2cnt_map.emplace_back(std::make_pair(m_last_ts, m_seen));
            m_last_ts = ts;
        }
    }

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

    if (m_enable_frequency_estimation)
    {
        m_last_ts = 0;
        m_tmp_cnt_ts = 0;
        m_tmp_cnt_map.clear();
        m_ts2cnt_map.clear();
    }
}

size_t
SamplingSketch::memory_usage() const
{
    size_t sum = 24 + sizeof(m_rng);
    for (unsigned i = 0; i < m_sample_size; ++i)
    {
        sum += m_reservoir[i].memory_usage();
    }
    if (m_enable_frequency_estimation)
    {
        sum += sizeof(m_last_ts)
            + sizeof(m_ts2cnt_map)
            + m_ts2cnt_map.capacity() * sizeof(m_ts2cnt_map[0]);
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

uint64_t
SamplingSketch::estimate_frequency(
    TIMESTAMP ts_e,
    uint32_t key) const
{
    if (m_tmp_cnt_ts != ts_e)
    {
        unsigned n = (unsigned) std::min((unsigned long long) m_sample_size, m_seen);
        unsigned nvalid = 0;
        m_tmp_cnt_map.clear();
        for (unsigned i = 0; i < n; ++i)
        {
            Item *item = m_reservoir[i].last_of(ts_e);
            if (item)
            {
                ++nvalid;
                ++m_tmp_cnt_map[item->m_value.m_u32];
            }
        }
        
        if (nvalid != 0)
        {
            auto iter = std::upper_bound(m_ts2cnt_map.begin(), m_ts2cnt_map.end(),
                ts_e, [](TIMESTAMP ts, const auto &p) -> bool
                {
                    return ts < p.first;
                });
            --iter;
            uint64_t tot_cnt = iter->second;
            double scale_factor = (double) tot_cnt / nvalid;
            for (auto &p: m_tmp_cnt_map)
            {
                p.second = std::round(p.second * scale_factor);
            }
        }
        
        m_tmp_cnt_ts = ts_e;
    }
    
    auto iter = m_tmp_cnt_map.find(key);
    if (iter == m_tmp_cnt_map.end())
    {
        return 0;
    }
    return iter->second;
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
    bool in_frequency_estimation_test;

    sample_size = g_config->get_u32("SAMPLING.sample_size", idx).value();
    seed = g_config->get_u32("SAMPLING.seed", -1).value();

    in_frequency_estimation_test = 
        (g_config->get("test_name").value() == "frequency_estimation");

    return new SamplingSketch(sample_size, seed, in_frequency_estimation_test);
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

const dsimpl::AVLNodeDescByOffset<SamplingSketchBITP::Item, TIMESTAMP>
SamplingSketchBITP::m_ts_map_node_desc(
    SSBITP_ITEM_OFFSET_OF(m_ts),
    SSBITP_ITEM_OFFSET_OF(m_payload) + 0,
    SSBITP_ITEM_OFFSET_OF(m_payload) + sizeof(Item*),
    SSBITP_ITEM_OFFSET_OF(m_payload) + 2 * sizeof(Item*),
    SSBITP_ITEM_OFFSET_OF(m_payload) + 6 * sizeof(Item*));

const dsimpl::AVLNodeDescByOffset<SamplingSketchBITP::Item, double>
SamplingSketchBITP::m_weight_map_node_desc(
    SSBITP_ITEM_OFFSET_OF(m_min_weight_list.m_weight),
    SSBITP_ITEM_OFFSET_OF(m_payload) + 3 * sizeof(Item*),
    SSBITP_ITEM_OFFSET_OF(m_payload) + 4 * sizeof(Item*),
    SSBITP_ITEM_OFFSET_OF(m_payload) + 5 * sizeof(Item*),
    SSBITP_ITEM_OFFSET_OF(m_payload) + 6 * sizeof(Item*) + sizeof(int));

#define SSBITP_ITEMNEW_OFFSET_OF(member) \
    (dsimpl::PayloadOffset) offsetof(SamplingSketchBITP::ItemNew, member) - \
    (dsimpl::PayloadOffset) offsetof(SamplingSketchBITP::ItemNew, m_payload)
SamplingSketchBITP::ItemNewNodeDesc::ItemNewNodeDesc(
    uint32_t sample_size):
    dsimpl::AVLNodeDescByOffset<SamplingSketchBITP::ItemNew, TIMESTAMP>(
        SSBITP_ITEMNEW_OFFSET_OF(m_ts),
        SSBITP_ITEMNEW_OFFSET_OF(m_tree_left),
        SSBITP_ITEMNEW_OFFSET_OF(m_tree_right),
        SSBITP_ITEMNEW_OFFSET_OF(m_tree_parent),
        SSBITP_ITEMNEW_OFFSET_OF(m_tree_hdiff)),
    m_sample_size(sample_size),
    m_tot_num_samples(0)
{
}

void
SamplingSketchBITP::ItemNewNodeDesc::_fix_agg(
    ItemNew *item,
    void *info,
    dsimpl::Type2AggregateOps op)
{
    
    ItemNew *item2;

    switch (op) {
    case dsimpl::T2AGGOPS_INSERTION:
        item2 = (ItemNew*) info;
        if (item->m_samples->size() < m_sample_size)
        {
            item->m_samples->insert(item2); 
            ++m_tot_num_samples;
        } else {
            auto smallest_item_iter = find_smallest_item(item);
            if (itemnew_total_order_comp_func(*smallest_item_iter, item2))
            {
                item->m_samples->erase(smallest_item_iter);
                item->m_samples->insert(item2);
            }
        }
        break;

    case dsimpl::T2AGGOPS_DELETION:
        item2 = (ItemNew*) info;
        {
            auto iter = item->m_samples->lower_bound(item2);
            if (iter != item->m_samples->end() && (*iter)->m_weight == item2->m_weight)
            {
                // Highly unlikely but we need to search for the actual item2 if
                // there're duplciate weights. Since it should be very rare,
                // we don't bother optimize for that by enforcing the strict total
                // order among items in the m_samples tree.
                if (*iter != item2)
                {
                    do {
                        ++iter;
                        if (iter == item->m_samples->end() ||
                            (*iter)->m_weight != item2->m_weight)
                        {
                            goto T2AGGOPS_DELETION_done;
                        }
                    } while (*iter != item2);
                }
                
                item->m_samples->erase(iter);

                if ((_left(item) ? _left(item)->m_samples->size() : 0) +
                    (_right(item) ? _right(item)->m_samples->size() : 0) +
                     1 == item->m_samples->size())
                {
                    --m_tot_num_samples;
                    goto T2AGGOPS_DELETION_done; 
                }

                // now we need to replace item2 with other items in the
                // subtree if available
                
                // iter is the one with the smallest weight, but again, we need
                // to check if there's one that comes later with the same
                // weight.
                iter = item->m_samples->begin();
                auto iter2 = iter;
                while (++iter2 != item->m_samples->end() &&
                        (*iter2)->m_weight == (*iter)->m_weight)
                {
                    if ((*iter2)->m_seq_no > (*iter)->m_seq_no)
                    {
                        iter = iter2;
                    }
                }
                
                ItemNew *smallest_item = *iter;
                ItemNew *candidate = nullptr;
        
                auto check_subtree_for_candidate = [&smallest_item]
                (ItemNew *node) -> ItemNew*
                {
                    ItemNew *candidate2 = nullptr;
                    auto iter = node->m_samples->lower_bound(smallest_item->m_weight);
                    if (iter != node->m_samples->end() &&
                        (*iter)->m_weight == smallest_item->m_weight)
                    {
                        auto iter2 = iter;
                        // check for duplicate weights
                        do
                        {
                            if ((*iter)->m_seq_no > smallest_item->m_seq_no)
                            {
                                if (!candidate2 ||
                                    (*iter)->m_seq_no < candidate2->m_seq_no)
                                {
                                    candidate2 = *iter;
                                }
                            }
                            ++iter;
                        } while (iter != node->m_samples->end()
                            && (*iter)->m_weight == smallest_item->m_weight);
                        if (candidate2)
                        {
                            return candidate2;
                        }
                        iter = iter2;
                    }
                    
                    // riter points to one position prior to iter
                    auto riter = decltype(node->m_samples->rbegin())(iter);
                    if (riter == node->m_samples->rend())
                    {
                        // This is specific to dsimpl::avl_container where one position
                        // prior to its begin() is end().
                        return nullptr; 
                    }
                    candidate2 = *riter;
                    while (++riter != node->m_samples->rend() &&
                            (*riter)->m_weight == candidate2->m_weight)
                    {
                        // check for duplicate weights
                        if ((*riter)->m_seq_no < candidate2->m_seq_no)
                        {
                            candidate2 = *riter;
                        }
                    }
                    return candidate2;    
                };

                // the first candidate: the upper bound of weight on the left
                if (_left(item))
                {
                    candidate = check_subtree_for_candidate(_left(item));
                    if (candidate && candidate->m_weight == smallest_item->m_weight)
                    {
                        goto T2AGGOPS_DELETION_candidate_found;
                    }
                }

                // the second candidate: subtree root
                if (itemnew_total_order_comp_func(item, smallest_item))
                {
                    if (!candidate ||
                        itemnew_total_order_comp_func(candidate, item))
                    {
                        candidate = item;
                        if (candidate->m_weight == smallest_item->m_weight)
                        {
                            goto T2AGGOPS_DELETION_candidate_found;
                        }
                    }
                } // otherwise it should be already in the sample

                // the third candidate: the upper bound of weight on the right hand side
                if (_right(item))
                {
                    auto candidate2 = check_subtree_for_candidate(_right(item));
                    if (!candidate ||
                        itemnew_total_order_comp_func(candidate, candidate2))
                    {
                        candidate = candidate2;
                    }
                }

T2AGGOPS_DELETION_candidate_found:
                if (candidate) item->m_samples->insert(candidate);
            }
        }
T2AGGOPS_DELETION_done:
        break;

    case dsimpl::T2AGGOPS_DUPLICATE:
        item2 = (ItemNew*) info;
        std::swap(item->m_samples, item2->m_samples);
        break;

    case dsimpl::T2AGGOPS_RECONSTRUCT:
        m_tot_num_samples -= item->m_samples->size();
        item->m_samples->clear();
        if (!_left(item))
        {
            if (!_right(item))
            {
                item->m_samples->insert(item);
            }
            else
            {
                reconstruct_samples_one_side(item, _right(item));
            }
        }
        else
        {
            if (!_right(item))
            {
                reconstruct_samples_one_side(item, _left(item));
            }
            else
            {
                reconstruct_samples_two_sides(item);
            }
        }

        break;

    case dsimpl::T2AGGOPS_APPLY_DELTA:
        assert(false); // unreacheable in our impl.
        break;
    }
}

SamplingSketchBITP::ItemNewNodeDesc::sample_iterator
SamplingSketchBITP::ItemNewNodeDesc::find_smallest_item(
    ItemNew *root)
{
    auto iter = root->m_samples->begin();
    auto smallest_item_iter = iter;
    while (++iter != root->m_samples->end() &&
        (*iter)->m_weight == (*smallest_item_iter)->m_weight)
    {
        if ((*iter)->m_seq_no > (*smallest_item_iter)->m_seq_no)
        {
            smallest_item_iter = iter;
        }
    }
    return smallest_item_iter;
}

void
SamplingSketchBITP::ItemNewNodeDesc::reconstruct_samples_one_side(
    ItemNew *root,
    ItemNew *subtree)
{
    assert(root->m_samples->empty());
    auto riter = subtree->m_samples->rbegin();
    double last_weight = 0;
    while (riter != subtree->m_samples->rend())
    {
        if (root->m_samples->size() < m_sample_size)
        {
            last_weight = (*riter)->m_weight;
            root->m_samples->insert(*riter);
        }
        else
        {
            if ((*riter)->m_weight < last_weight)
            {
                break;
            }
            assert((*riter)->m_weight == last_weight);
            auto smallest_item_iter = find_smallest_item(root);
            if ((*riter)->m_seq_no < (*smallest_item_iter)->m_seq_no)
            {
                root->m_samples->erase(smallest_item_iter);
                root->m_samples->insert(*riter);
            }
        }
        ++riter;
    }
    
    if (root->m_samples->size() < m_sample_size)
    {
        root->m_samples->insert(root);
    }
    else if (root->m_weight >= last_weight)
    {
        auto smallest_item_iter = find_smallest_item(root);
        if (itemnew_total_order_comp_func(*smallest_item_iter, root))
        {
            root->m_samples->erase(smallest_item_iter);
            root->m_samples->insert(root);
        }
    }

    m_tot_num_samples += root->m_samples->size();
}

void
SamplingSketchBITP::ItemNewNodeDesc::reconstruct_samples_two_sides(
    ItemNew *root)
{
    assert(root->m_samples->empty());
    auto left = _left(root);
    auto right = _right(root);
    auto iter_l = left->m_samples->rbegin();
    auto iter_r = right->m_samples->rbegin();
    while (iter_l != left->m_samples->rend() &&
            iter_r != right->m_samples->rend())
    {
        if ((*iter_l)->m_weight > (*iter_r)->m_weight)
        {
            root->m_samples->insert(*iter_l);
            ++iter_l;
            if (root->m_samples->size() == m_sample_size)
            {
                break;
            }
        }
        else
        {
            root->m_samples->insert(*iter_r);
            ++iter_r;
            if (root->m_samples->size() == m_sample_size)
            {
                break;
            }
        }
    }
    
    while (iter_l != left->m_samples->rend())
    {
        if (root->m_samples->size() < m_sample_size)
        {
            root->m_samples->insert(*iter_l);
        }
        else
        {
            auto smallest_item_iter = find_smallest_item(root); 
            if ((*smallest_item_iter)->m_weight == (*iter_l)->m_weight)
            {
                if ((*smallest_item_iter)->m_seq_no > (*iter_l)->m_seq_no)
                {
                    root->m_samples->erase(smallest_item_iter);
                    root->m_samples->insert(*iter_l);
                }
            }
            else
            {
                assert((*smallest_item_iter)->m_weight > (*iter_l)->m_weight);
                break;
            }
        }
        ++iter_l;
    }

    while (iter_r != right->m_samples->rend())
    {
        if (root->m_samples->size() < m_sample_size)
        {
            root->m_samples->insert(*iter_r);
        }
        else
        {
            auto smallest_item_iter = find_smallest_item(root);
            if ((*smallest_item_iter)->m_weight == (*iter_r)->m_weight)
            {
                if ((*smallest_item_iter)->m_seq_no > (*iter_r)->m_seq_no)
                {
                    root->m_samples->erase(smallest_item_iter);
                    root->m_samples->insert(*iter_r);
                }
            }
            else
            {
                assert((*smallest_item_iter)->m_weight > (*iter_r)->m_weight);
                break;
            }
        }
        ++iter_r;
    }
    
    if (root->m_samples->size() < m_sample_size)
    {
        root->m_samples->insert(root);
    }
    else
    {
        auto smallest_item_iter = find_smallest_item(root);
        if (itemnew_total_order_comp_func(*smallest_item_iter, root))
        {
            root->m_samples->erase(smallest_item_iter);
            root->m_samples->insert(root);
        }
    }

    m_tot_num_samples += root->m_samples->size();
}

SamplingSketchBITP::ItemNewNodeDesc2::ItemNewNodeDesc2():
    dsimpl::AVLNodeDescByOffset<SamplingSketchBITP::ItemNew, double>(
        SSBITP_ITEMNEW_OFFSET_OF(m_weight),
        SSBITP_ITEMNEW_OFFSET_OF(m_tree_left),
        SSBITP_ITEMNEW_OFFSET_OF(m_tree_right),
        SSBITP_ITEMNEW_OFFSET_OF(m_tree_parent),
        SSBITP_ITEMNEW_OFFSET_OF(m_tree_hdiff)) {
}

void
SamplingSketchBITP::ItemNewNodeDesc2::_fix_agg(ItemNew *item) const {
    item->m_cnt_field = 1;
    if (_left(item)) item->m_cnt_field += _left(item)->m_cnt_field;
    if (_right(item)) item->m_cnt_field += _right(item)->m_cnt_field;
}

const dsimpl::AVLNodeDescByOffset<SamplingSketchBITP::ItemNew, double>
SamplingSketchBITP::m_itemnew_node_desc3(
    SSBITP_ITEMNEW_OFFSET_OF(m_min_weight),
    SSBITP_ITEMNEW_OFFSET_OF(m_tree_left2),
    SSBITP_ITEMNEW_OFFSET_OF(m_tree_right2),
    SSBITP_ITEMNEW_OFFSET_OF(m_tree_parent2),
    SSBITP_ITEMNEW_OFFSET_OF(m_tree_hdiff2));

SamplingSketchBITP::SamplingSketchBITP(
    uint32_t sample_size,
    uint32_t seed,
    uint8_t use_new_impl,
    bool enable_frequency_estimation):
    m_sample_size(sample_size),
    m_use_new_impl(use_new_impl),
    m_enable_frequency_estimation(enable_frequency_estimation),
    // use_new_impl == 0
    m_ts_map(m_ts_map_node_desc),
    m_min_weight_map(m_weight_map_node_desc),
    m_most_recent_items_weight_map(m_weight_map_node_desc),
    m_most_recent_items((use_new_impl == 0) ? new Item*[m_sample_size] : nullptr),
    m_most_recent_items_start(0),
    m_rng(seed),
    m_unif_0_1(0.0, 1.0),
    m_num_items_alloced(0),
    m_num_mwlistnodes_alloced(0),
    // use_new_impl == 1 (broken)
    m_recent_items_new((use_new_impl == 1) ? new ItemNew*[m_sample_size]: nullptr),
    m_recent_items_start_new(m_sample_size - 1),
    m_ts_map_new(ItemNewNodeDesc(
        m_sample_size)),
    m_recent_items_weight_map(ItemNewNodeDesc2()),
    m_min_weight_map_new(m_itemnew_node_desc3),
    m_next_seq_no(0),
    m_num_itemnew_alloced(0),
    // use_new_impl == 2
    m_item3_head(nullptr),
    m_num_item3_alloced(0),
    m_num_item3_alloced_target(0),
    m_tot_seen(0),
    m_max_num_item3_alloced(0),
    // frequency estimation
    m_last_ts(0),
    m_tmp_cnt_ts(~0ul),
    m_tmp_cnt_map(),
    m_ts2cnt_map()
{
    if (m_use_new_impl == 0)
    {
        std::memset(m_most_recent_items, 0, sizeof(Item*) * m_sample_size);
    }
    else if (m_use_new_impl == 1)
    {
        std::memset(m_recent_items_new, 0, sizeof(ItemNew*) * m_sample_size);
    } else if (m_use_new_impl == 2)
    {
        // in case m_sample_size == 1
        m_num_item3_alloced_target =
            2 * m_sample_size * std::ceil(std::log(m_sample_size + 1));
    }
}

SamplingSketchBITP::~SamplingSketchBITP()
{
    clear();
    delete[] m_most_recent_items;
    delete[] m_recent_items_new;
}

void
SamplingSketchBITP::clear()
{
    if (m_use_new_impl == 0)
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
                //--m_num_mwlistnodes_alloced;
                delete n2;
            }
            assert(m_num_items_alloced);
            //--m_num_items_alloced;
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
                //--m_num_items_alloced;
                delete m_most_recent_items[i]; 
                m_most_recent_items[i] = nullptr;
            }
        }
        
        //assert(m_num_items_alloced == 0);
        //assert(m_num_mwlistnodes_alloced == 0);

        m_num_items_alloced = 0;
        m_num_mwlistnodes_alloced = 0;
    }
    else if (m_use_new_impl == 1)
    {
        for (uint32_t i = 0; i < m_sample_size; ++i)
        {
            if (m_recent_items_new[i])
            {
                delete m_recent_items_new[i];
                m_recent_items_new[i] = nullptr;
            }
        }
        m_recent_items_start_new = m_sample_size - 1;

        m_recent_items_weight_map.clear();
        m_min_weight_map_new.clear();
        m_ts_map_new.delete_tree(
            [this](ItemNew *item) {
                delete item->m_samples;
                delete item;
            });

        m_next_seq_no = 0;
        m_num_itemnew_alloced = 0;
        m_ts_map_new.m_tot_num_samples = 0;
    }
    else if (m_use_new_impl == 2)
    {
        while (m_item3_head)
        {
            Item3 *item3 = m_item3_head;
            m_item3_head = m_item3_head->m_next;
            delete item3;
        }
        m_num_item3_alloced = 0;
        m_num_item3_alloced_target = 2 * m_sample_size *
            (uint64_t) std::ceil(std::log(m_sample_size));
        m_tot_seen = 0;
        m_max_num_item3_alloced = 0;
    }

    if (m_enable_frequency_estimation)
    {
        m_last_ts = 0;
        m_tmp_cnt_ts = ~0ul;
        m_tmp_cnt_map.clear();
        m_ts2cnt_map.clear();
    }
}

size_t
SamplingSketchBITP::memory_usage() const
{
    if (m_use_new_impl == 0)
    {
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
    else if (m_use_new_impl == 1)
    {
        return 8 // m_sample_size, m_use_new_impl, m_itemnew_alloc and alignment
            + sizeof(m_rng)
            + sizeof(m_unif_0_1)
            + sizeof(ItemNew**) + m_sample_size * sizeof(ItemNew*) // m_recent_items_new
            + sizeof(ptrdiff_t)
            + sizeof(m_ts_map_new)
            + sizeof(m_recent_items_weight_map)
            + sizeof(m_min_weight_map_new)
            + sizeof(uint64_t) * 2 // m_next_seq_no and m_num_itemnew_alloced
            + m_num_itemnew_alloced * sizeof(ItemNew)
            + m_ts_map_new.m_tot_num_samples *
                sizeof(dsimpl::avl_container_impl::Node<ItemNew*, void>);
    }
    else if (m_use_new_impl == 2)
    {
        size_t sum = 40 // m_item3_head, m_num_item3_alloced,
                  // m_num_item3_alloced_target, m_tot_seen
                  // m_max_num_item3_alloced
            + m_num_item3_alloced * sizeof(Item3)
            + sizeof(m_unif_0_1)
            + sizeof(m_rng);

        if (m_enable_frequency_estimation)
        {
            sum += sizeof(m_last_ts)
                + sizeof(m_ts2cnt_map)
                + m_ts2cnt_map.capacity() * sizeof(m_ts2cnt_map[0]);
        }
        return sum;
    }
    
    assert(false);
    return 0;
}

size_t
SamplingSketchBITP::max_memory_usage() const
{
    if (m_use_new_impl == 2)
    {
        size_t sum = 40 // m_item3_head, m_num_item3_alloced,
                  // m_num_item3_alloced_target, m_tot_seen
                  // m_max_num_item3_alloced
            + m_max_num_item3_alloced * sizeof(Item3)
            + sizeof(m_unif_0_1)
            + sizeof(m_rng);

        if (m_enable_frequency_estimation)
        {
            sum += sizeof(m_last_ts)
                + sizeof(m_ts2cnt_map)
                + m_ts2cnt_map.capacity() * sizeof(m_ts2cnt_map[0]);
        }
        return sum;
    }

    assert(false);
    return memory_usage(); // not implemented for 0 and 1
}

std::string
SamplingSketchBITP::get_short_description() const
{
    return std::string("SAMPLING_BITP-ss") + std::to_string(m_sample_size) +
        "-use_new_impl-" + std::to_string(m_use_new_impl);
}

void
SamplingSketchBITP::update(
    TIMESTAMP ts,
    uint32_t value,
    int c)
{
    if (m_use_new_impl == 0)
    {
        update_old(ts, value, c);
    }
    else if (m_use_new_impl == 1)
    {
        update_new(ts, value, c);
    }
    else if (m_use_new_impl == 2)
    {
        update_batched(ts, value, c);
    }
    else
    {
        assert(false);
    }
}

std::vector<IPersistentHeavyHitterSketchBITP::HeavyHitter>
SamplingSketchBITP::estimate_heavy_hitters_bitp(
    TIMESTAMP ts_s,
    double frac_threshold) const
{
    if (m_use_new_impl == 0)
    {
        return estimate_heavy_hitters_bitp_old(ts_s, frac_threshold);
    }
    else if (m_use_new_impl == 1)
    {
        return estimate_heavy_hitters_bitp_new(ts_s, frac_threshold);
    }
    else if (m_use_new_impl == 2)
    {
        return estimate_heavy_hitters_bitp_batched(ts_s, frac_threshold);
    }

    assert(false);
    return std::vector<IPersistentHeavyHitterSketchBITP::HeavyHitter>();
}

void
SamplingSketchBITP::update_new(
    TIMESTAMP ts,
    uint32_t value,
    int c)
{
    static uint64_t n_seen = 0;

update_loop:
    double weight = m_unif_0_1(m_rng);
    
    // items are totally ordered by the pair (m_weight, m_seq_no),
    // in increasing m_weight and decreasing m_seq_no number
    ItemNew *item = new ItemNew;
    item->m_ts = ts;
    item->m_value = value;
    item->m_weight = weight;
    // item->m_samples is not constructed nor is item->m_seq_no assigned until
    // the new item is added to the m_ts_map_new
    item->m_samples = nullptr;
    // TODO remove the following line after debugging
    item->m_seq_no = m_next_seq_no++;
    ++m_num_itemnew_alloced;

    std::cout << "ItemNew " << n_seen++ << ": " << item->m_ts << ' ' << item->m_value
        << ' ' << item->m_weight << std::endl;
    
    ItemNew *purgeable_item = nullptr; // the (m_sample_size + 1)^th most recent item
    m_recent_items_start_new = (m_sample_size - 1 == m_recent_items_start_new) ?
            0 : (m_recent_items_start_new + 1);
    if (m_recent_items_new[m_sample_size - 1])
    {
        purgeable_item = m_recent_items_new[m_recent_items_start_new];
        m_recent_items_weight_map.erase(purgeable_item);
    }
    m_recent_items_new[m_recent_items_start_new] = item;

    // now determine if purgeable_item can be safely purged
    if (purgeable_item)
    {
        // purgeable_item->m_cnt_field = # of the recent items with weight <= its weight
        purgeable_item->m_cnt_field = (decltype(purgeable_item->m_cnt_field))
            m_recent_items_weight_map.get_sum_left<true>(
                purgeable_item->m_weight, SSBITP_ITEMNEW_OFFSET_OF(m_cnt_field));
        assert(purgeable_item->m_cnt_field < m_sample_size);
        if (purgeable_item->m_cnt_field == 0 &&
            item->m_weight > purgeable_item->m_weight)
        {
            delete purgeable_item;
            --m_num_itemnew_alloced;
        }
        else
        {
            auto lm = m_recent_items_weight_map.begin_node();
            purgeable_item->m_min_weight = lm->m_weight;
            // TODO uncomment the following
            //purgeable_item->m_seq_no = m_next_seq_no++;
            purgeable_item->m_samples =
                new dsimpl::avl_container<ItemNew*, double (*)(ItemNew*)>{
                    itemnew_weight_key_func 
                };
            m_ts_map_new.insert(purgeable_item);
            m_min_weight_map_new.insert(purgeable_item);

            // The min_weight is adjusted in the next loop if item->m_weight >
            // lm->m_weight
        }

    }
    m_recent_items_weight_map.insert(item);
    
    // purge earlier items
    {
        auto iter = m_min_weight_map_new.begin();
        ItemNew *reinsertion_list = nullptr;
        ItemNew **reinsertion_list_tail_ptr = &reinsertion_list;
        while (iter != m_min_weight_map_new.end() && iter->m_min_weight < item->m_weight)
        {
            ItemNew *purgeable_item = iter.get_node();
            iter = m_min_weight_map_new.erase(iter);
            if (purgeable_item->m_cnt_field == 0)
            {
                // this item can be safely dicarded             
                m_ts_map_new.erase(purgeable_item);
                m_ts_map_new.m_tot_num_samples -= purgeable_item->m_samples->size();
                delete purgeable_item->m_samples;
                delete purgeable_item;
                --m_num_itemnew_alloced;
            }
            else
            {
                recompute_min_weight(purgeable_item, item);
                *reinsertion_list_tail_ptr = purgeable_item;
                // we borrow the parent field in m_min_weight_map_new
                // as the next ptr in the reinsertion list
                m_min_weight_map_new._parent(purgeable_item) = nullptr;
                reinsertion_list_tail_ptr =
                    &m_min_weight_map_new._parent(purgeable_item);
            }
        }

        while (reinsertion_list)
        {
            ItemNew *item = reinsertion_list;
            reinsertion_list = m_min_weight_map_new._parent(reinsertion_list);
            m_min_weight_map_new.insert(item);
        }
    }

    if (--c) goto update_loop;
}

template<bool has_nonnull_excluded>
void
SamplingSketchBITP::find_sample_set_exclude_impl(
    SampleSet *sset,
    ItemNew *excluded)
{
    assert(sset->m_pending_combined);

    auto combined = sset->m_pending_combined;
    sset->m_pending_combined = nullptr;
    auto combined_iter = combined->m_samples->rbegin();
    decltype(combined_iter) excluded_iter;
    if constexpr (has_nonnull_excluded)
    {
        excluded_iter = excluded->m_samples->rbegin();
    }

    auto find_next_included_item = [&]() -> ItemNew* {
        if constexpr (has_nonnull_excluded)
        {
            while (combined_iter != combined->m_samples->rend())
            {
                while (excluded_iter != excluded->m_samples->rend() &&
                    (*excluded_iter)->m_weight > (*combined_iter)->m_weight)
                {
                    ++excluded_iter;        
                }

                auto item = *combined_iter;
                ++combined_iter; // combined_iter incremented here for the loop
                if (excluded_iter != excluded->m_samples->rend() &&
                    (*excluded_iter)->m_weight == item->m_weight) {
                
                    // highly unlikely but it might be duplicate weights but different
                    // nodes
                    auto excluded_iter2 = excluded_iter;
                    while (*excluded_iter2 != item) {
                        ++excluded_iter2;
                        if (excluded_iter2 == excluded->m_samples->rend() ||
                            (*excluded_iter2)->m_weight < item->m_weight)
                        {
                            // item is not actually in the subtree at excluded,
                            // which makes item a valid one to be included
                            return item;
                        }
                    }
                    // finishing the above while loop means we have found item
                    // in the exclusion list
                }
                else
                {
                    // no match, item is included
                    return item;
                }
            }
        }
        else
        {
            if (combined_iter != combined->m_samples->rend())
            {
                auto item = *combined_iter;
                ++combined_iter;
                return item;
            }
        }
    
        // we've reached the end of the combined list
        return nullptr;
    };
    
    ItemNew *item = find_next_included_item();
    while (sset->m_cur_size < sset->m_sample_size)
    {
        if (!item) return ;
        sset->m_min_heap[sset->m_cur_size] = item;
        if (++sset->m_cur_size == sset->m_sample_size) {
            // we don't have to make the min heap until we have m_sample_size items
            dsimpl::min_heap_make(
                sset->m_min_heap,
                sset->m_sample_size,
                itemnew_id_key_func,
                itemnew_total_order_comp_func);
        }
        item = find_next_included_item();
    }
    
    while (item)
    {
        if (itemnew_total_order_comp_func(sset->m_min_heap[0], item)) {
            sset->m_min_heap[0] = item;
            dsimpl::min_heap_push_down(
                sset->m_min_heap,
                0,
                sset->m_sample_size,
                itemnew_id_key_func,
                itemnew_total_order_comp_func);
            item = find_next_included_item();
        }
        else
        {
            break;
        }
    }
}

SamplingSketchBITP::SampleSet *
SamplingSketchBITP::find_sample_set(
    ItemNew *item) const
{
    // XXX this is completely wrong
    // TODO need to maintain accumulated left sample count + top m_sample_size
    // samples in each node and query recursively
    
    auto sset = (SampleSet *) new char[
        sizeof(SampleSet) + sizeof(ItemNew*) * m_sample_size];
    sset->m_cur_size = 0;
    sset->m_sample_size = m_sample_size;
    sset->m_pending_combined = nullptr;

    m_ts_map_new.get_sum_right<true>(
        item,
        sset,
        find_sample_set_combine,
        find_sample_set_exclude);

    if (sset->m_pending_combined)
    {
        find_sample_set_exclude_impl<false>(sset, nullptr);
    }

    auto iter = m_recent_items_weight_map.rbegin();
    if (sset->m_cur_size < m_sample_size)
    {
        do
        {
            if (iter == m_recent_items_weight_map.rend()) return sset;
            sset->m_min_heap[sset->m_cur_size++] = iter.get_node();
            ++iter;
        } while (sset->m_cur_size < m_sample_size);

        if (iter == m_recent_items_weight_map.rend()) return sset;
        dsimpl::min_heap_make(
            sset->m_min_heap,
            m_sample_size,
            itemnew_id_key_func,
            itemnew_total_order_comp_func);
    }

    while (iter != m_recent_items_weight_map.rend() &&
            itemnew_total_order_comp_func(sset->m_min_heap[0], iter.get_node()))
    {
        sset->m_min_heap[0] = iter.get_node();
        dsimpl::min_heap_push_down(
            sset->m_min_heap,
            0,
            m_sample_size,
            itemnew_id_key_func,
            itemnew_total_order_comp_func);
        ++iter;
    }

    return sset;
}

void
SamplingSketchBITP::find_sample_set_combine(
    SampleSet *sset,
    ItemNew *item)
{
    // In the current AVL implementation, a combine may or may not be followed
    // by an exclude. In the case it is followed by an exclude, we defer that
    // until find_sample_set_exclude is callled with some non-null item.
    // Otherwise, we apply that pending combined item and defer the current
    // combined item.
    if (sset->m_pending_combined)
    {
        find_sample_set_exclude_impl<false>(sset, nullptr);
    }
    assert(!sset->m_pending_combined);
    sset->m_pending_combined = item;
}

void
SamplingSketchBITP::find_sample_set_exclude(
    SampleSet *sset,
    ItemNew *item)
{
    assert(sset->m_pending_combined);
    assert(item);
    find_sample_set_exclude_impl<true>(sset, item);
}

void
SamplingSketchBITP::destroy_sample_set(
    SampleSet *sset)
{
    delete[] (char*) sset;
}

void
SamplingSketchBITP::recompute_min_weight(
    ItemNew *item,
    ItemNew *inserted_item) const {
    
    SampleSet *sset = find_sample_set(item);

    // we won't call recompute_min_weight() on the most recent items
    assert(sset->m_cur_size == m_sample_size); 
   
    item->m_min_weight = sset->m_min_heap[0]->m_weight;
    if (item->m_weight < inserted_item->m_weight)
    {
        --item->m_cnt_field;
    }

#if !defined(NDEBUG)
    uint32_t recomputed_cnt_field = 0;
    for (uint32_t i = 0; i < sset->m_cur_size; ++i) {
        if (itemnew_total_order_comp_func(sset->m_min_heap[i], item))
        {
            ++recomputed_cnt_field;
        }
    }
    assert(recomputed_cnt_field == item->m_cnt_field);
#endif

    destroy_sample_set(sset);
}

std::vector<IPersistentHeavyHitterSketchBITP::HeavyHitter>
SamplingSketchBITP::estimate_heavy_hitters_bitp_new(
    TIMESTAMP ts_s,
    double frac_threshold) const
{
    auto iter = m_ts_map_new.lower_bound(ts_s);
    auto item = iter.get_node();
    SampleSet *sset = find_sample_set(item);

    std::unordered_map<uint32_t, uint32_t> cnt_map;
    for (uint32_t i = 0; i < sset->m_cur_size; ++i)
    {
        ++cnt_map[sset->m_min_heap[i]->m_value];
    }

    std::vector<HeavyHitter> ret;
    uint32_t m = sset->m_cur_size;
    auto sample_threshold = (uint32_t) std::floor(frac_threshold * m);
    for (const auto &p: cnt_map)
    {
        if (p.second > sample_threshold)
        {
            ret.emplace_back(HeavyHitter{p.first, (float) p.second / m});
        }
    }

    destroy_sample_set(sset);

    return std::move(ret);
}

void
SamplingSketchBITP::update_old(
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
                    ++m_num_mwlistnodes_alloced;
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
                    ++m_num_mwlistnodes_alloced;
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
SamplingSketchBITP::estimate_heavy_hitters_bitp_old(
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
    auto sample_threshold = (uint32_t) std::floor(frac_threshold * m);
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

void
SamplingSketchBITP::update_batched(
    TIMESTAMP ts,
    uint32_t value,
    int c)
{
    if (m_enable_frequency_estimation)
    {
        if (ts == m_tmp_cnt_ts)
        {
            m_tmp_cnt_ts = ~0ul;
            m_tmp_cnt_map.clear();
        }

        if (ts != m_last_ts)
        {
            m_ts2cnt_map.emplace_back(std::make_pair(m_last_ts, m_tot_seen));
            m_last_ts = ts;
        }
    }

update_loop:
    double weight = m_unif_0_1(m_rng);

    Item3 *item3 = new Item3{
        ts,
        value,
        weight,
        m_item3_head
    };
    m_item3_head = item3;
    ++m_tot_seen;
    ++m_num_item3_alloced;
    if (m_num_item3_alloced > m_max_num_item3_alloced)
    {
        m_max_num_item3_alloced = m_num_item3_alloced;
    }
    
    assert(m_num_item3_alloced <= m_num_item3_alloced_target);
    if (m_num_item3_alloced == m_num_item3_alloced_target)
    {
        std::vector<Item3*> min_heap;
        min_heap.reserve(m_sample_size);

        Item3 *item3 = m_item3_head;
        Item3 **prev_next_ptr = &m_item3_head;
        while (min_heap.size() < m_sample_size)
        {
            assert(item3);
            min_heap.push_back(item3);
            prev_next_ptr = &item3->m_next;
            item3 = item3->m_next;
        }

        dsimpl::min_heap_make(
            min_heap,
            m_sample_size,
            [](Item3 *item3) -> double { return item3->m_weight; });

        while (item3)
        {
            if (item3->m_weight < min_heap[0]->m_weight)
            {
                // don't keep
                item3 = item3->m_next; 
                delete *prev_next_ptr;
                *prev_next_ptr = item3;
                --m_num_item3_alloced;
            }
            else
            {
                // replace the min weight
                min_heap[0] = item3;
                dsimpl::min_heap_push_down(
                    min_heap,
                    0,
                    m_sample_size,
                    [](Item3 *item3) -> double { return item3->m_weight; });
                prev_next_ptr = &item3->m_next;
                item3 = item3->m_next;
            }
        }

        m_num_item3_alloced_target = 2 * m_sample_size
            * (uint64_t) std::ceil(std::log(m_tot_seen + 1));
        if (m_num_item3_alloced_target < 2 * m_num_item3_alloced)
        {
            m_num_item3_alloced_target = 2 * m_num_item3_alloced;
        }
    }

    if (--c) goto update_loop;
}

std::vector<IPersistentHeavyHitterSketchBITP::HeavyHitter>
SamplingSketchBITP::estimate_heavy_hitters_bitp_batched(
    TIMESTAMP ts_s,
    double frac_threshold) const
{
    std::vector<Item3*> samples;
    samples.reserve(m_sample_size);
    
    Item3 *item3 = m_item3_head;
    while (item3 && item3->m_ts > ts_s && samples.size() < m_sample_size)
    {
        samples.push_back(item3);
        item3 = item3->m_next;
    }

    if (item3 && item3->m_ts > ts_s)
    {
        dsimpl::min_heap_make(
            samples,
            (dsimpl::UINT8) samples.size(),
            [](Item3 *item3) -> double { return item3->m_weight; });
        while (item3 && item3->m_ts > ts_s)
        {
            if (item3->m_weight >= samples[0]->m_weight)
            {
                samples[0] = item3;
                dsimpl::min_heap_push_down(
                    samples,
                    0,
                    m_sample_size,
                    [](Item3 *item3) -> double { return item3->m_weight; });
            }

            item3 = item3->m_next;
        }
    }

    std::unordered_map<uint32_t, uint32_t> cnt_map;
    for (Item3 *item: samples)
    {
        ++cnt_map[item->m_value];
    }
    
    std::vector<IPersistentHeavyHitterSketchBITP::HeavyHitter> ret;
    uint32_t m = (uint32_t) samples.size();
    auto sample_threshold = (uint32_t) std::floor(frac_threshold * m);
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

uint64_t
SamplingSketchBITP::estimate_frequency_bitp(
    TIMESTAMP ts_s,
    uint32_t key) const
{
    if (m_tmp_cnt_ts != ts_s)
    {
        std::vector<Item3*> samples;
        samples.reserve(m_sample_size);
        
        Item3 *item3 = m_item3_head;
        while (item3 && item3->m_ts > ts_s && samples.size() < m_sample_size)
        {
            samples.push_back(item3);
            item3 = item3->m_next;
        }

        if (item3 && item3->m_ts > ts_s)
        {
            dsimpl::min_heap_make(
                samples,
                (dsimpl::UINT8) samples.size(),
                [](Item3 *item3) -> double { return item3->m_weight; });
            while (item3 && item3->m_ts > ts_s)
            {
                if (item3->m_weight >= samples[0]->m_weight)
                {
                    samples[0] = item3;
                    dsimpl::min_heap_push_down(
                        samples,
                        0,
                        m_sample_size,
                        [](Item3 *item3) -> double { return item3->m_weight; });
                }

                item3 = item3->m_next;
            }
        }
        
        uint32_t nvalid = 0;
        m_tmp_cnt_map.clear();
        for (Item3 *item: samples)
        {
            ++nvalid;
            ++m_tmp_cnt_map[item->m_value];
        }

        if (nvalid != 0)
        {
            auto iter = std::upper_bound(m_ts2cnt_map.begin(), m_ts2cnt_map.end(),
                ts_s, [](TIMESTAMP ts, const auto &p) -> bool
                {
                    return ts < p.first;
                });
            --iter;
            uint64_t tot_cnt = iter->second;
            double scale_factor = (double) tot_cnt / nvalid;
            for (auto &p: m_tmp_cnt_map)
            {
                p.second = std::round(p.second * scale_factor);
            }
        }

        m_tmp_cnt_ts = ts_s;
    }
    
    auto iter = m_tmp_cnt_map.find(key);
    if (iter == m_tmp_cnt_map.end())
    {
        return 0;
    }
    return iter->second;
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

    uint32_t use_new_impl;
    if (!g_config->is_list("SAMPLING_BITP.use_new_impl"))
    {
        use_new_impl = g_config->get_u32("SAMPLING_BITP.use_new_impl").value();
    }
    else
    {
        use_new_impl = g_config->get_u32("SAMPLING_BITP.use_new_impl", idx).value();
    }

    if (use_new_impl > 2)
    {
        std::cerr << "[WARN] invalid value of SAMPLING_BITP.use_new_impl, defaults to 0"
            << std::endl;
    }

    bool in_frequency_estimation_test = 
        (g_config->get("test_name").value() == "frequency_estimation_bitp");
    if (in_frequency_estimation_test && use_new_impl != 2)
    {
        std::cerr << "[ERROR] frequnecy_estimation_bitp with SAMPLING_BITP is only implemented with use_new_impl == 2, but got " << use_new_impl
            << std::endl;
        return nullptr;
    }

    return new SamplingSketchBITP(sample_size, seed, (uint8_t) use_new_impl,
            in_frequency_estimation_test);
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

#ifndef NDEBUG
void
__attribute__((unused,noinline))
SamplingSketchBITP::print_tree(
    dsimpl::avl_t<ItemNewNodeDesc, false> *avl) {
    avl->print([](std::ostream &out, ItemNew *item) {
        out << item->m_weight;         
    });
}
#endif



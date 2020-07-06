#include "pmmg.h"
#include "conf.h"
#include <cmath>
#include <numeric>
#include <cassert>
#include <iostream>
#include <list>

using dsimpl::UINT8;

namespace MisraGriesSketches {

struct MisraGriesAccessor {
    static cnt_map_t&
    cnt_map(MG *mg) { return mg->m_cnt; }

    static const cnt_map_t&
    cnt_map(const MG *mg) { return mg->m_cnt; }

    static void
    reset_delta(MG *mg) { mg->reset_delta(); }

    static uint64_t&
    delta(MG *mg) { return mg->m_delta; }

    static uint64_t&
    min_cnt(MG *mg) { return mg->m_min_cnt; }

    static void
    update_impl(
        MG                      *mg,
        uint32_t                element,
        int                     cnt,
        std::list<uint32_t>     *p_deleted_list)
    {
        mg->update_impl(element, cnt, p_deleted_list);
    }

    static uint64_t
    get_cnt(MG *mg, uint32_t key)
    {
        return (mg->m_cnt.find(key) == mg->m_cnt.end()) ? 0 : mg->m_cnt[key];
    }

    static uint64_t
    get_cnt_prime(MG *mg, uint32_t key)
    {
        auto iter = mg->m_cnt.find(key);
        if (iter == mg->m_cnt.end()) return 0;
        if (iter->second < mg->m_delta) return 0;
        return iter->second - mg->m_delta;
    }
};

using MGA = MisraGriesAccessor;

//
// ChainMisraGries implementation
// 

ChainMisraGries::ChainMisraGries(
    double  epsilon,
    bool    use_update_new):
    m_epsilon(epsilon),
    m_epsilon_over_3(epsilon / 3),
    m_k((uint32_t) std::ceil(1 / m_epsilon_over_3)),
    m_use_update_new(use_update_new),
    m_tot_cnt(0u),
    m_last_ts(0u),
    m_cur_sketch(m_k),
    m_snapshot_cnt_map((cnt_map_t::size_type) std::ceil(umap_default_load_factor * (m_k > 1 ? m_k - 1: 1))),
    m_checkpoints(),
    m_delta_list_tail_ptr(nullptr),
    m_num_delta_nodes_since_last_chkpt(0),
    m_sub_amount(0),
    m_all_counters(nullptr),
    m_free_counters(nullptr),
    m_key_to_counter_map(),
    m_deleted_counters(),
    m_c1_max_heap(),
    m_c2_min_heap(),
    m_inverted_index_proxy(),
    m_tmp_cnt_ts(0),
    m_tmp_cnt_map()
{
    m_all_counters = new Counter[2 * (m_k - 1)];
    
    m_free_counters = m_all_counters;
    for (uint64_t i = 1; i < 2 * (m_k - 1); ++i)
    {
        m_all_counters[i-1].m_next_free_counter = &m_all_counters[i];
        m_all_counters[i-1].m_in_last_snapshot = false;
        m_all_counters[i-1].m_last_node_is_chkpt = false;
    }
    m_all_counters[2 * m_k - 3].m_next_free_counter = nullptr;
    m_all_counters[2 * m_k - 3].m_in_last_snapshot = false;
    m_all_counters[2 * m_k - 3].m_last_node_is_chkpt = false;
}

ChainMisraGries::~ChainMisraGries()
{
    clear();
    delete [] m_all_counters;
}

void
ChainMisraGries::clear()
{
    m_tot_cnt = 0;
    m_last_ts = 0;
    m_cur_sketch.clear();
    m_snapshot_cnt_map.clear();
    for (ChkptNode &chkpt: m_checkpoints)
    {
        clear_delta_list(chkpt.m_first_delta_node);
    }
    m_checkpoints.clear();
    m_delta_list_tail_ptr = nullptr;
    m_num_delta_nodes_since_last_chkpt = 0;

    m_free_counters = m_all_counters;
    for (uint64_t i = 1; i < 2 * (m_k - 1); ++i)
    {
        m_all_counters[i-1].m_next_free_counter = &m_all_counters[i];
        m_all_counters[i-1].m_in_last_snapshot = false;
        m_all_counters[i-1].m_last_node_is_chkpt = false;
    }
    m_all_counters[2 * m_k - 3].m_next_free_counter = nullptr;
    m_all_counters[2 * m_k - 3].m_in_last_snapshot = false;
    m_all_counters[2 * m_k - 3].m_last_node_is_chkpt = false;

    m_sub_amount = 0;
    m_key_to_counter_map.clear();
    m_c1_max_heap.clear();
    m_c2_min_heap.clear();
    m_deleted_counters.clear();
    m_tmp_cnt_ts = 0;
    m_tmp_cnt_map.clear();
}

size_t
ChainMisraGries::memory_usage() const
{
    size_t size_of_checkpoints_and_dnodes = 
        sizeof(m_checkpoints) + sizeof(ChkptNode) * m_checkpoints.capacity() +
        std::accumulate(m_checkpoints.cbegin(), m_checkpoints.cend(),
            (size_t) 0,
            [](size_t acc, const ChkptNode &chkpt) -> size_t {
                DeltaNode *n = chkpt.m_first_delta_node;
                while (n)
                {
                    acc += sizeof(DeltaNode);
                    n = n->m_next;
                }
                acc += size_of_unordered_map(chkpt.m_cnt_map);
                return acc;
            });

    if (m_use_update_new)
    {
        return 88 + // scalar members + m_inverted_index_proxy (which counts as 8 for alignment)
            m_cur_sketch.memory_usage() + 
            size_of_checkpoints_and_dnodes +
            sizeof(Counter) * (2 * m_k - 2) +
            size_of_unordered_map(m_key_to_counter_map) +
            size_of_unordered_map(m_deleted_counters) +
            sizeof(m_c1_max_heap) + sizeof(Counter*) * m_c1_max_heap.capacity() +
            sizeof(m_c2_min_heap) + sizeof(Counter*) * m_c2_min_heap.capacity();
    }
    
    return 56 + // scalar members
        m_cur_sketch.memory_usage() +
        size_of_unordered_map(m_snapshot_cnt_map) +
        size_of_checkpoints_and_dnodes;
}

std::string
ChainMisraGries::get_short_description() const
{
    return "CMG-e" + std::to_string(m_epsilon);
}

void
ChainMisraGries::update(
    TIMESTAMP           ts,
    uint32_t            key,
    int                 c)
{
    if (ts == m_tmp_cnt_ts)
    {
        m_tmp_cnt_ts = 0;
        m_tmp_cnt_map.clear();
    }

    if (m_use_update_new)
        update_new(ts, key, c);
    else
        update_old(ts, key, c);
}

void
ChainMisraGries::update_new(
    TIMESTAMP           ts,
    uint32_t            key,
    int                 c)
{
    assert(c > 0);

    if (m_last_ts > 0 && ts != m_last_ts)
    {
        if (m_checkpoints.empty())
        {
            make_checkpoint();
        }
        else
        {
            int64_t d = (int64_t)(m_sub_amount + MGA::delta(&m_cur_sketch));
    
            bool do_make_checkpoint = false;
            DeltaNode *new_dnode_list = nullptr;
            DeltaNode **new_dnode_list_tail_ptr = &new_dnode_list;
            size_t num_deltas = m_num_delta_nodes_since_last_chkpt;
            size_t max_allowable_num_deltas =
                (size_t) std::log2(m_tot_cnt) / m_epsilon_over_3;

            int64_t max_delta_c = (int64_t) get_allowable_cnt_upper_bound(0, m_tot_cnt);

            // find keys whose counts exceed the upper limit
            while (m_c1_max_heap[0]->m_c1 > d)
            {
                if (++num_deltas > max_allowable_num_deltas)
                {
                    do_make_checkpoint = true;
                    goto check_for_checkpoint;
                }

                Counter *p_counter = m_c1_max_heap[0];
                assert(p_counter->m_c2 >= d);
            
                uint64_t c_prime = MGA::get_cnt_prime(&m_cur_sketch, p_counter->m_key);
                //uint64_t c_prime_alt = ((p_counter->m_c1 + p_counter->m_c2) >> 1) +
                //    p_counter->m_c - d;
                //assert(c_prime == c_prime_alt);

                DeltaNode *n = new DeltaNode;
                n->m_ts = m_last_ts;
                n->m_tot_cnt = m_tot_cnt;
                n->m_key = p_counter->m_key;
                n->m_new_cnt = c_prime;
                n->m_next = nullptr;
                *new_dnode_list_tail_ptr = n;
                new_dnode_list_tail_ptr = &n->m_next;

                p_counter->m_c1 = d - max_delta_c;
                m_inverted_index_proxy.m_updating_c1_max_heap = true;
                dsimpl::max_heap_with_inverted_index_push_down(
                    m_c1_max_heap,
                    m_inverted_index_proxy,
                    0,
                    (UINT8) m_c1_max_heap.size(),
                    c1_max_heap_key_func,
                    heap_idx_func);

                p_counter->m_c2 = d + max_delta_c;
                m_inverted_index_proxy.m_updating_c1_max_heap = false;
                dsimpl::min_heap_with_inverted_index_mark_updated(
                    m_c2_min_heap,
                    m_inverted_index_proxy,
                    p_counter->m_c2_min_heap_idx,
                    (UINT8) m_c2_min_heap.size(),
                    c2_min_heap_key_func,
                    heap_idx_func);
                
                p_counter->m_in_last_snapshot = true;
                p_counter->m_last_node_is_chkpt = false;
                p_counter->m_prev_delta_node = n;
            }
        
            // find keys whose counts drop below the lower limit
            while (m_c2_min_heap[0]->m_c2 < d)
            {
                if (++num_deltas > max_allowable_num_deltas)
                {
                    do_make_checkpoint = true;
                    goto check_for_checkpoint;
                }

                Counter *p_counter = m_c2_min_heap[0];
                assert(p_counter->m_c1 <= d);

                uint64_t c_prime = MGA::get_cnt_prime(&m_cur_sketch, p_counter->m_key);
                /*uint64_t c_prime_alt =
                    ((p_counter->m_c1 + p_counter->m_c2) >> 1) + p_counter->m_c; */

                DeltaNode *n = new DeltaNode;
                n->m_ts = m_last_ts;
                n->m_tot_cnt = m_tot_cnt;
                n->m_key = p_counter->m_key;
                n->m_new_cnt = c_prime;
                n->m_next = nullptr;
                *new_dnode_list_tail_ptr = n;
                new_dnode_list_tail_ptr = &n->m_next;

                p_counter->m_c1 = d - max_delta_c;
                m_inverted_index_proxy.m_updating_c1_max_heap = true;
                dsimpl::max_heap_with_inverted_index_mark_updated(
                    m_c1_max_heap,
                    m_inverted_index_proxy,
                    (UINT8) p_counter->m_c1_max_heap_idx,
                    (UINT8) m_c1_max_heap.size(),
                    c1_max_heap_key_func,
                    heap_idx_func);

                p_counter->m_c2 = d + max_delta_c;
                m_inverted_index_proxy.m_updating_c1_max_heap = false;
                dsimpl::min_heap_with_inverted_index_push_down(
                    m_c2_min_heap,
                    m_inverted_index_proxy,
                    (UINT8) 0,
                    (UINT8) m_c2_min_heap.size(),
                    c2_min_heap_key_func,
                    heap_idx_func);

                p_counter->m_in_last_snapshot = true;
                p_counter->m_last_node_is_chkpt = false;
                p_counter->m_prev_delta_node = n;
            }

            // record deleted nodes as well
            for (auto iter2 = m_deleted_counters.begin();
                      iter2 != m_deleted_counters.end();
                      ++iter2)
            {
                if (++num_deltas > max_allowable_num_deltas)
                {
                    do_make_checkpoint = true;
                    // can't goto check_for_checkpoint because
                    // make_checkpoint could be adding counters prior to this
                    // one in the list a second time
                }

                Counter *p_counter = iter2->second;
                assert(p_counter->m_in_last_snapshot);
                assert(m_key_to_counter_map.count(p_counter->m_key) == 0);

                assert(MGA::cnt_map(&m_cur_sketch).find(p_counter->m_key) == MGA::cnt_map(&m_cur_sketch).end());
                
                if (!do_make_checkpoint)
                {
                    DeltaNode *n = new DeltaNode;
                    n->m_ts = m_last_ts;
                    n->m_tot_cnt = m_tot_cnt;
                    n->m_key = p_counter->m_key;
                    n->m_new_cnt = 0;
                    n->m_next = nullptr;
                    *new_dnode_list_tail_ptr = n;
                    new_dnode_list_tail_ptr = &n->m_next;
                }

                p_counter->m_in_last_snapshot = false;
                p_counter->m_next_free_counter = m_free_counters;
                m_free_counters = p_counter;
            }
            m_deleted_counters.clear();

check_for_checkpoint:
            if (do_make_checkpoint)
            {
                clear_delta_list(new_dnode_list);
                make_checkpoint();
            }
            else if (new_dnode_list)
            {
                *m_delta_list_tail_ptr = new_dnode_list;
                m_delta_list_tail_ptr = new_dnode_list_tail_ptr;
                m_num_delta_nodes_since_last_chkpt = num_deltas;
            }
        }
    }
     
    auto &cnt_map = MGA::cnt_map(&m_cur_sketch);
    assert(cnt_map.size() == m_key_to_counter_map.size());

    auto iter = cnt_map.find(key);
    uint64_t c0, c1;
    if (iter == cnt_map.end())
    {
        c0 = 0;
    }
    else
    {
        c0 = iter->second;
    }
    
    std::list<uint32_t> deleted_list;
    //uint64_t delta_sub_amount = m_sub_amount;
    uint64_t old_delta = MGA::delta(&m_cur_sketch);
    //MGA::update_impl(&m_cur_sketch, key, c, &m_sub_amount, &deleted_list);
    MGA::update_impl(&m_cur_sketch, key, c, &deleted_list);
    //delta_sub_amount = m_sub_amount - delta_sub_amount;
    uint64_t new_delta = MGA::delta(&m_cur_sketch);
    
    iter = cnt_map.find(key);
    if (iter == cnt_map.end())
    {
        c1 = 0;
    }
    else
    {
        c1 = iter->second;
    }

    m_sub_amount += c0 + c - c1 + old_delta - new_delta;
    assert(c1 <= c0 + c);
    //assert(c0 + c - c1 == new_delta - old_delta + delta_sub_amount);
    
    // now counters are in sync with m_cur_sketch.m_cnt
    for (uint32_t deleted_key : deleted_list)
    {
        auto iter2 = m_key_to_counter_map.find(deleted_key);
        assert(iter2 != m_key_to_counter_map.end());

        Counter *p_counter = iter2->second;

        m_inverted_index_proxy.m_updating_c1_max_heap = true;
        dsimpl::max_heap_with_inverted_index_remove(
            m_c1_max_heap,
            m_inverted_index_proxy,
            p_counter->m_c1_max_heap_idx,
            (UINT8) m_c1_max_heap.size(),
            c1_max_heap_key_func,
            heap_idx_func);
        m_c1_max_heap.pop_back();

        m_inverted_index_proxy.m_updating_c1_max_heap = false;
        dsimpl::min_heap_with_inverted_index_remove(
            m_c2_min_heap,
            m_inverted_index_proxy,
            p_counter->m_c2_min_heap_idx,
            (UINT8) m_c2_min_heap.size(),
            c2_min_heap_key_func,
            heap_idx_func);
        m_c2_min_heap.pop_back();

        m_key_to_counter_map.erase(iter2);
        if (p_counter->m_in_last_snapshot)
        {
            m_deleted_counters[p_counter->m_key] = p_counter;
        }
        else
        {
            p_counter->m_next_free_counter = m_free_counters;
            m_free_counters = p_counter;
        }
    }
    
    if (c1 > 0)
    {
        auto iter2 = m_key_to_counter_map.find(key);
        if (iter2 == m_key_to_counter_map.end())
        {
            uint64_t c2 = c1 + m_sub_amount;
            Counter *p_counter;
            auto iter3 = m_deleted_counters.find(key);
            if (iter3 != m_deleted_counters.end())
            {
                p_counter = iter3->second;
                m_deleted_counters.erase(iter3);
                
                int64_t c;
                int64_t max_delta_c;
                if (p_counter->m_last_node_is_chkpt)
                {
                    auto iter = p_counter->m_prev_chkpt_node->m_cnt_map.find(key);
                    assert(iter != p_counter->m_prev_chkpt_node->m_cnt_map.end());
                    c = iter->second;
                    max_delta_c = (int64_t) get_allowable_cnt_upper_bound(
                            0, p_counter->m_prev_chkpt_node->m_tot_cnt);
                }
                else
                {
                    c = p_counter->m_prev_delta_node->m_new_cnt;
                    max_delta_c = (int64_t) get_allowable_cnt_upper_bound(
                            0, p_counter->m_prev_delta_node->m_tot_cnt);
                }
                p_counter->m_c1 = (int64_t) c2 - c - max_delta_c;
                p_counter->m_c2 = (int64_t) c2 - c + max_delta_c;
            }
            else
            {
                p_counter = m_free_counters;
                m_free_counters = m_free_counters->m_next_free_counter;
                p_counter->m_key = key;
                    
                int64_t max_delta_c = (int64_t) get_allowable_cnt_upper_bound(
                        0, tot_cnt_at_last_chkpt());
                p_counter->m_c1 = (int64_t) c2 - max_delta_c;
                p_counter->m_c2 = (int64_t) c2 + max_delta_c;
            }

            m_key_to_counter_map[key] = p_counter;
            
            m_c1_max_heap.push_back(p_counter); 
            m_inverted_index_proxy.m_updating_c1_max_heap = true;
            dsimpl::max_heap_with_inverted_index_push_up(
                m_c1_max_heap,
                m_inverted_index_proxy,
                (UINT8)(m_c1_max_heap.size() - 1),
                (UINT8) m_c1_max_heap.size(),
                c1_max_heap_key_func,
                heap_idx_func);

            m_c2_min_heap.push_back(p_counter);
            m_inverted_index_proxy.m_updating_c1_max_heap = false;
            dsimpl::min_heap_with_inverted_index_push_up(
                m_c2_min_heap,
                m_inverted_index_proxy,
                (UINT8)(m_c2_min_heap.size() - 1),
                (UINT8) m_c2_min_heap.size(),
                c2_min_heap_key_func,
                heap_idx_func);
        }
        else
        {
            // key found in m_counters
            Counter *p_counter = iter2->second;
            p_counter->m_c1 += c;
            m_inverted_index_proxy.m_updating_c1_max_heap = true;
            dsimpl::max_heap_with_inverted_index_push_up(
                m_c1_max_heap,
                m_inverted_index_proxy,
                (UINT8) p_counter->m_c1_max_heap_idx,
                (UINT8) m_c1_max_heap.size(),
                c1_max_heap_key_func,
                heap_idx_func); 

            p_counter->m_c2 += c;
            m_inverted_index_proxy.m_updating_c1_max_heap = false;
            dsimpl::min_heap_with_inverted_index_push_up(
                m_c2_min_heap,
                m_inverted_index_proxy,
                (UINT8) p_counter->m_c2_min_heap_idx,
                (UINT8) m_c2_min_heap.size(),
                c2_min_heap_key_func,
                heap_idx_func);
        }
    }
    
    m_tot_cnt += c;
    m_last_ts = ts;
}

void
ChainMisraGries::update_old(
    TIMESTAMP           ts,
    uint32_t            key,
    int                 c)
{
    if (m_last_ts > 0 && ts != m_last_ts)
    {
        // ts change, find delta changes and make checkpoint if necessary
        if (MGA::delta(&m_cur_sketch) != 0)
        {
            MGA::reset_delta(&m_cur_sketch);
        }
        if (m_checkpoints.empty())
        {
            // first checkpoint
            make_checkpoint_old();
        }
        else
        {
            // find delta changes
            auto &cur_cnt_map = MGA::cnt_map(&m_cur_sketch);
            
            bool do_make_checkpoint = false;
            DeltaNode *new_delta_list = nullptr;
            DeltaNode **new_delta_list_tail_ptr = &new_delta_list;
            size_t num_deltas = m_num_delta_nodes_since_last_chkpt;
            size_t max_allowable_num_deltas = (size_t) std::log2(m_tot_cnt);
            for (auto iter_snapshot = m_snapshot_cnt_map.begin();
                    iter_snapshot != m_snapshot_cnt_map.end(); )
            {
                auto iter_cur = cur_cnt_map.find(iter_snapshot->first);
                if (iter_cur == cur_cnt_map.end())
                {
                    if (++num_deltas > max_allowable_num_deltas)
                    {
                        do_make_checkpoint = true;
                        break;
                    }

                    DeltaNode *n = new DeltaNode;
                    n->m_ts = m_last_ts;
                    n->m_tot_cnt = m_tot_cnt;
                    n->m_key = iter_snapshot->first;
                    n->m_new_cnt = 0;
                    n->m_next = nullptr;
                    *new_delta_list_tail_ptr = n;
                    new_delta_list_tail_ptr = &n->m_next;
                    iter_snapshot = m_snapshot_cnt_map.erase(iter_snapshot);
                }
                else
                {
                    ++iter_snapshot;
                }
            }
        
            if (!do_make_checkpoint)
            {
                for (const auto &p: cur_cnt_map)
                {
                    auto iter_snapshot = m_snapshot_cnt_map.find(p.first);
                    bool do_make_delta = false;
                    if (iter_snapshot == m_snapshot_cnt_map.end())
                    {
                        do_make_delta = p.second > (uint64_t) std::floor(
                            m_checkpoints.back().m_tot_cnt * m_epsilon_over_3);
                    }
                    else
                    {
                        auto range = get_allowable_approx_cnt_range(iter_snapshot->second);
                        do_make_delta = p.second < range.first || p.second > range.second;
                    }

                    if (do_make_delta)
                    {
                        if (++num_deltas > max_allowable_num_deltas)
                        {
                            do_make_checkpoint = true;
                            break;
                        }
                    
                        DeltaNode *n = new DeltaNode;
                        n->m_ts = m_last_ts;
                        n->m_tot_cnt = m_tot_cnt;
                        n->m_key = p.first;
                        n->m_new_cnt = p.second;
                        n->m_next = nullptr;
                        *new_delta_list_tail_ptr = n;
                        new_delta_list_tail_ptr = &n->m_next;
                        m_snapshot_cnt_map[p.first] = SnapshotCounter{
                            p.second, m_tot_cnt  
                        };
                    }
                }
            }

            if (do_make_checkpoint)
            {
                clear_delta_list(new_delta_list);
                make_checkpoint_old();
            }
            else if (new_delta_list)
            {
                *m_delta_list_tail_ptr = new_delta_list;
                m_delta_list_tail_ptr = new_delta_list_tail_ptr;
                m_num_delta_nodes_since_last_chkpt = num_deltas;
            }
        }
    }

    m_cur_sketch.update(key, c);
    m_last_ts = ts;
    m_tot_cnt += c;
}

std::vector<IPersistentHeavyHitterSketch::HeavyHitter>
ChainMisraGries::estimate_heavy_hitters(
    TIMESTAMP ts_e,
    double frac_threshold) const
{
    if (ts_e >= m_last_ts)
    {
        return m_cur_sketch.estimate_heavy_hitters(frac_threshold, m_tot_cnt);
    }
    
    create_tmp_cnt_at(ts_e);    
    
    uint64_t threshold = (uint64_t) std::ceil((m_epsilon_over_3 + frac_threshold - m_epsilon) * m_est_tot_cnt);
    std::vector<HeavyHitter> ret;
    for (auto &p: m_tmp_cnt_map)
    {
        if (p.second >= threshold)
        {
            ret.emplace_back(HeavyHitter{
                p.first, (float)((double) p.second / m_est_tot_cnt - m_epsilon_over_3)
            });
        }
    }

    return std::move(ret);
}

void
ChainMisraGries::create_tmp_cnt_at(
    TIMESTAMP ts_e) const
{
    m_tmp_cnt_ts = ts_e;
    m_tmp_cnt_map.clear();

    auto iter = std::upper_bound(
        m_checkpoints.begin(), m_checkpoints.end(),
        ts_e,
        [](TIMESTAMP ts_e, const ChkptNode &n) -> bool {
            return ts_e < n.m_ts;
        });

    if (iter == m_checkpoints.begin())
    {
        m_est_tot_cnt = 0;
        return;
    }

    const ChkptNode &last_chkpt = *(iter - 1);

    std::unordered_map<key_type, uint64_t> &snapshot = m_tmp_cnt_map;
    uint64_t d = (uint64_t) std::floor(last_chkpt.m_tot_cnt * m_epsilon_over_3);
    for (const auto &p: last_chkpt.m_cnt_map)
    {
        if (p.second > d)
        {
            snapshot[p.first] = p.second - d;
        }
    }
    
    TIMESTAMP prev_ts = last_chkpt.m_ts;
    uint64_t prev_tot_cnt = last_chkpt.m_tot_cnt;

    DeltaNode *n = last_chkpt.m_first_delta_node;
    while (n)
    {
        if (n->m_ts > ts_e) break;
        if (n->m_new_cnt == 0)
        {
            snapshot.erase(n->m_key);
        }
        else
        {
            d = (uint64_t) std::floor(n->m_tot_cnt * m_epsilon_over_3);
            if (n->m_new_cnt > d)
            {
                snapshot[n->m_key] = n->m_new_cnt - d;
            }
            else
            {
                snapshot.erase(n->m_key);
            }
        }
        prev_ts = n->m_ts;
        prev_tot_cnt = n->m_tot_cnt;
        n = n->m_next;
    }
    
    TIMESTAMP next_ts;
    uint64_t next_tot_cnt;
    if (!n)
    {
        if (iter == m_checkpoints.end())
        {
            next_ts = m_last_ts;
            next_tot_cnt = m_tot_cnt;
        }
        else
        {
            next_ts = iter->m_ts;
            next_tot_cnt = iter->m_tot_cnt;
        }
    }
    else
    {
        next_ts = n->m_ts;
        next_tot_cnt = n->m_tot_cnt;
    }
    
    // Obsolete: m_tot_cnt is an underestimate of m_tot_cnt at timestamp ts_e and it
    // can deviate by an arbitrarily large number
    
    // We now estimate tot_cnt_at_ts as a linear interpolation with its previous and next stored counts
    if (prev_ts == next_ts)
    {
        assert(ts_e == prev_ts);
        m_est_tot_cnt = iter->m_tot_cnt;
    }
    else
    {
        m_est_tot_cnt = (
            (next_tot_cnt - prev_tot_cnt) * 1.0 * ts_e +
            prev_tot_cnt * 1.0 * next_ts -
            next_tot_cnt * 1.0 * prev_ts) / (next_ts - prev_ts);
    }
}

uint64_t
ChainMisraGries::estimate_frequency(
    TIMESTAMP ts_e,
    uint32_t key) const
{
    const std::unordered_map<key_type, uint64_t> &cnt_map =
        (ts_e >= m_last_ts) ? MGA::cnt_map(&m_cur_sketch) : m_tmp_cnt_map;
    if (ts_e < m_last_ts && m_tmp_cnt_ts != ts_e)
    {
        create_tmp_cnt_at(ts_e);
    }
    
    // realign the error bound to center around the true value
    // d = +eps / 6 * N
    uint64_t d = m_epsilon_over_3 / 2 * m_est_tot_cnt;
    auto iter = cnt_map.find(d);
    if (iter == cnt_map.end()) return 0;
    uint64_t est_c = iter->second;
    return est_c + d;
}

void
ChainMisraGries::make_checkpoint_old()
{
    m_checkpoints.emplace_back(ChkptNode {
        m_last_ts,
        m_tot_cnt,
        nullptr,
        MGA::cnt_map(&m_cur_sketch)
    });
    m_delta_list_tail_ptr = &m_checkpoints.back().m_first_delta_node;
    m_num_delta_nodes_since_last_chkpt = 0;
    
    m_snapshot_cnt_map.clear();
    for (const auto &p: m_checkpoints.back().m_cnt_map)
    {
        m_snapshot_cnt_map.insert_or_assign(p.first, SnapshotCounter{
            p.second, m_tot_cnt 
        });
    }
}

void
ChainMisraGries::make_checkpoint()
{
    if (MGA::delta(&m_cur_sketch) != 0)
    {
        MGA::reset_delta(&m_cur_sketch);
    }
    m_checkpoints.emplace_back(ChkptNode {
        m_last_ts,
        m_tot_cnt,
        nullptr,
        MGA::cnt_map(&m_cur_sketch)
    });
    m_delta_list_tail_ptr = &m_checkpoints.back().m_first_delta_node;
    m_num_delta_nodes_since_last_chkpt = 0;
    
    // XXX doesn't reduce memory usage
    // reset load factor to the default value
    //cnt_map_t &cnt_map = m_checkpoints.back().m_cnt_map;
    //cnt_map.max_load_factor(1 / umap_default_load_factor);
    //cnt_map.rehash(cnt_map.size() * umap_default_load_factor);

    m_sub_amount = 0;
    int64_t max_delta_c = (int64_t) get_allowable_cnt_upper_bound(0, m_tot_cnt);
    for (Counter *p_counter : m_c1_max_heap)
    {
        p_counter->m_c1 = -max_delta_c;
        p_counter->m_c2 = max_delta_c;
        p_counter->m_in_last_snapshot = true;
        p_counter->m_last_node_is_chkpt = true;
        p_counter->m_prev_chkpt_node = &m_checkpoints.back();
    }
    
    for (auto &p: m_deleted_counters)
    {
        Counter *p_counter = p.second;
        p_counter->m_in_last_snapshot = false;
        p_counter->m_next_free_counter = m_free_counters;
        m_free_counters = p_counter;
    }
    m_deleted_counters.clear();
}

ChainMisraGries*
ChainMisraGries::get_test_instance()
{
    return new ChainMisraGries(0.1);
}

ChainMisraGries*
ChainMisraGries::create_from_config(
    int idx)
{
    double epsilon = g_config->get_double("CMG.epsilon", idx).value();
    
    bool use_update_new;
    if (!g_config->is_list("CMG.use_update_new"))
    {
        use_update_new = g_config->get_boolean("CMG.use_update_new").value(); 
    }
    else
    {
        use_update_new = g_config->get_boolean("CMG.use_update_new", idx).value();
    }

    return new ChainMisraGries(epsilon, use_update_new);
}

int
ChainMisraGries::num_configs_defined()
{
    if (g_config->is_list("CMG.epsilon")) 
    {
        int len = (int) g_config->list_length("CMG.epsilon");
        
        int len2 = (int) g_config->list_length("CMG.use_update_new");
        if (len2 != -1 && len2 != len)
        {
            std::cerr << "[WARN] CMG ignored because list length mismatch in params"
                << std::endl;
            return 0;
        }

        return len;
    }
    return -1;
}

void
ChainMisraGries::clear_delta_list(
    DeltaNode *n)
{
    while (n)
    {
        DeltaNode *n2 = n;
        n = n->m_next;
        delete n2;
    }
}

//
// TreeMisraGries implementation
//

TreeMisraGries::TreeMisraGries(
    double  epsilon):
    m_epsilon(epsilon),
    m_epsilon_prime(epsilon / 3.0),
    m_k((uint32_t) std::ceil(1 / m_epsilon_prime)),
    m_last_ts(0),
    m_tot_cnt(0),
    m_tree(64, nullptr), // this is the tallest tree we can have with 64-bit counters
    m_level(0),
    m_remaining_nodes_at_cur_level(2 * m_k),
    m_target_cnt(1),
    m_max_cnt_per_node_at_cur_level(1),
    m_cur_sketch(nullptr),
    m_size_counter(0)
{
    m_cur_sketch = new MisraGries(m_k);
}

TreeMisraGries::~TreeMisraGries()
{
    clear();
    delete m_cur_sketch;
}

void
TreeMisraGries::clear()
{
    m_last_ts = 0;
    m_tot_cnt = 0;
    
    for (unsigned i = 0; i < m_tree.size(); ++i)
    {
        clear_tree(m_tree[i]);
    }
    std::fill(m_tree.begin(), m_tree.end(), nullptr);

    m_level = 0;
    m_remaining_nodes_at_cur_level = 2 * m_k;
    m_target_cnt = 1;
    m_max_cnt_per_node_at_cur_level =1;
    m_cur_sketch->clear();
    m_size_counter = 0;
}

size_t
TreeMisraGries::memory_usage() const
{
    return 80 + // sclar members
        sizeof(m_tree) + m_tree.capacity() * sizeof(TreeNode*) +
        m_size_counter +  // tot size of tree nodes
        m_cur_sketch->memory_usage();
}

std::string
TreeMisraGries::get_short_description() const
{
    return "TMG-e" + std::to_string(m_epsilon);
}

void
TreeMisraGries::update(
    TIMESTAMP ts,
    uint32_t value,
    int c)
{
    if (m_last_ts > 0 && m_last_ts != ts)
    {
        if (//m_tot_cnt >= m_target_cnt ||
            m_tot_cnt + c >= m_target_cnt)
        {
            merge_cur_sketch();    
        }
    }
    
    m_cur_sketch->update(value, c);
    m_last_ts = ts;
    m_tot_cnt += c;
}


std::vector<IPersistentHeavyHitterSketch::HeavyHitter>
TreeMisraGries::estimate_heavy_hitters(
    TIMESTAMP ts_e,
    double frac_threshold) const
{
    MisraGries *mg;
    uint32_t level;
    uint64_t est_tot_cnt = 0;
    if (ts_e >= m_last_ts)
    {
        mg = m_cur_sketch->clone();
        level = m_level;
        est_tot_cnt = m_tot_cnt;
    }
    else
    {
        level = m_level;
        mg = new MisraGries(m_k);
        for (; level < m_tree.size(); ++level)
        {
            if (m_tree[level])
            {
                if (m_tree[level]->m_ts > ts_e) // > ?
                {
                    bool does_intersect = false;
                    TreeNode *tn = m_tree[level];
                    while (tn->m_left)
                    {
                        if (tn->m_left->m_ts > ts_e) // >
                        {
                            // right tree is not in range
                            tn = tn->m_left;
                        }
                        else
                        {
                            does_intersect = true;
                            mg->merge(tn->m_left->m_mg);

                            est_tot_cnt = tn->m_left->m_tot_cnt;
                            tn = tn->m_right;
                        }
                    }
                    if (does_intersect)
                    {
                        ++level;
                        break;
                    }
                }
                else
                {
                    // the entire tree is in range
                    est_tot_cnt = m_tree[level]->m_tot_cnt;
                    break;            
                }
            }
        }
    }

    for (; level < m_tree.size(); ++level)
    {
        if (m_tree[level])
        {
            assert(ts_e > m_tree[level]->m_ts);
            mg->merge(m_tree[level]->m_mg);
        }
    }
    // threshold = frac_threshold 
    //             - eps/3 (m_epsilon_prime) 
    //             - eps/3 (in misra gries)
    auto ret = mg->estimate_heavy_hitters(
            frac_threshold - m_epsilon_prime, est_tot_cnt);

    delete mg;
    return std::move(ret);
}

void
TreeMisraGries::merge_cur_sketch()
{
    TreeNode *tn = new TreeNode;
    tn->m_ts = m_last_ts;
    tn->m_tot_cnt = m_tot_cnt;
    tn->m_mg = new MisraGries(m_k);
    tn->m_left = tn->m_right = nullptr;
    
    if (MGA::delta(m_cur_sketch) != 0)
    {
        MGA::reset_delta(m_cur_sketch);
    }
    cnt_map_t &cnt_map = MGA::cnt_map(m_cur_sketch);
    auto purge_threshold = m_tot_cnt * m_epsilon_prime; // eps / 3 * m_tot_cnt
    for (auto iter = cnt_map.begin(); iter != cnt_map.end();)
    {
        if (iter->second >= purge_threshold)
        {
            tn->m_mg->update(iter->first, iter->second);
            iter = cnt_map.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
    if (cnt_map.size() == 0)
    {
        MGA::min_cnt(m_cur_sketch) = ~0ul;
    }

    //m_size_counter += sizeof(TreeNode) + tn->m_mg->memory_usage();

    uint32_t level = m_level;
    while (m_tree[level])
    {
        TreeNode *tn_merged = new TreeNode;
        tn_merged->m_ts = m_last_ts;
        tn_merged->m_tot_cnt = m_tot_cnt;

        //m_size_counter -= tn->m_mg->memory_usage();
        tn_merged->m_mg = tn->m_mg;
        tn_merged->m_mg->merge(m_tree[level]->m_mg);
        tn->m_mg = nullptr; // drop the mg sketch in the right child
        tn_merged->m_left = m_tree[level];
        tn_merged->m_right = tn;
        m_size_counter += sizeof(TreeNode);// + tn_merged->m_mg->memory_usage();

        m_tree[level] = nullptr;
        tn = tn_merged;
        ++level;

        if (level >= m_tree.size())
        {
            throw "Too many items inserted into TMG"; 
        }
    }
    
    m_size_counter += sizeof(TreeNode) + tn->m_mg->memory_usage();
    m_tree[level] = tn;

    if (--m_remaining_nodes_at_cur_level == 0)
    {
        ++m_level;
        //std::cout << "merge_cur_sketch(): " << m_level << ' ' << m_tot_cnt << std::endl;
        m_remaining_nodes_at_cur_level = m_k;
        m_max_cnt_per_node_at_cur_level *= 2;
    }

    m_target_cnt = m_tot_cnt + m_max_cnt_per_node_at_cur_level;
}

void
TreeMisraGries::clear_tree(
    TreeNode *root)
{
    if (!root) return ;
    delete root->m_mg;
    clear_tree(root->m_left);
    clear_tree(root->m_right);
    delete root;
}

int
TreeMisraGries::num_configs_defined()
{
    if (g_config->is_list("TMG.epsilon"))
    {
        int len = (int) g_config->list_length("TMG.epsilon");
        return len;
    }

    return -1;
}

TreeMisraGries*
TreeMisraGries::get_test_instance()
{
    return new TreeMisraGries(0.1);
}

TreeMisraGries*
TreeMisraGries::create_from_config(
    int idx)
{
    double epsilon = g_config->get_double("TMG.epsilon", idx).value();

    return new TreeMisraGries(epsilon);
}

//
// TreeMisraGriesBITP implementation
//

TreeMisraGriesBITP::TreeMisraGriesBITP(
    double  epsilon):
    m_epsilon(epsilon),
    m_epsilon_prime(epsilon / 3.0),
    m_k((uint32_t) std::ceil(1 / m_epsilon_prime)),
    m_last_ts(0),
    m_tot_cnt(0),
    m_tree(64, nullptr),
    m_right_most_nodes(64, nullptr),
    m_level(0),
    m_remaining_nodes_at_cur_level(2 * m_k),
    m_cur_sketch(nullptr),
    m_size_counter(0),
    m_size_counter_max(0),
    m_tmp_ts(~0ul),
    m_tmp_mg(nullptr),
    m_est_tot_cnt(0)
{
    m_cur_sketch = new MisraGries(m_k);
}

TreeMisraGriesBITP::~TreeMisraGriesBITP()
{
    clear();
    delete m_cur_sketch;
}

void
TreeMisraGriesBITP::clear()
{
    m_last_ts = 0;
    m_tot_cnt = 0;
    m_level = 0;
    m_remaining_nodes_at_cur_level = 0;
    m_cur_sketch->clear();
    m_size_counter = 0;
    m_size_counter_max = 0;

    for (size_t i = 0; i != m_right_most_nodes.size(); ++i)
    {
        TreeNode *n = m_right_most_nodes[i];
        if (n) {
            do {
                TreeNode *n2 = n;
                n = n->m_next;
                delete n2->m_mg;
                delete n2;
            } while (n != m_right_most_nodes[i]);
            m_right_most_nodes[i] = nullptr;
        }
        m_tree[i] = nullptr;
    }

    m_tmp_ts = ~0ul;
    if (m_tmp_mg)
    {
        delete m_tmp_mg;
        m_tmp_mg = nullptr;
    }
    m_est_tot_cnt = 0;
}

size_t
TreeMisraGriesBITP::memory_usage() const
{
    return 72 // scalar members
        + sizeof(m_tree) + m_tree.capacity() * sizeof(TreeNode*)
        + sizeof(m_right_most_nodes) + m_right_most_nodes.capacity() * sizeof(TreeNode*)
        + m_size_counter
        + m_cur_sketch->memory_usage();
}

size_t
TreeMisraGriesBITP::max_memory_usage() const
{
    return 72 // scalar members
        + sizeof(m_tree) + m_tree.capacity() * sizeof(TreeNode*)
        + sizeof(m_right_most_nodes) + m_right_most_nodes.capacity() * sizeof(TreeNode*)
        + m_size_counter_max
        + m_cur_sketch->memory_usage();
}

std::string
TreeMisraGriesBITP::get_short_description() const
{
    return "TMG_BITP-e" + std::to_string(m_epsilon);
}

void
TreeMisraGriesBITP::update(
    TIMESTAMP ts,
    uint32_t value,
    int c)
{
    if (m_tmp_ts == ts)
    {
        if (m_tmp_mg)
        {
            delete m_tmp_mg;
            m_tmp_mg = nullptr;
        }
        m_tmp_ts = ~0ul;
    }

    if (m_last_ts != 0 && m_last_ts != ts)
    {
        merge_cur_sketch(); 
    }

    if (m_size_counter > m_size_counter_max)
    {
        m_size_counter_max = m_size_counter;
    }

    m_cur_sketch->update(value, c);
    m_last_ts = ts;
    m_tot_cnt += c;
}

std::vector<IPersistentHeavyHitterSketchBITP::HeavyHitter>
TreeMisraGriesBITP::estimate_heavy_hitters_bitp(
    TIMESTAMP ts_s,
    double frac_threshold) const
{
    if (ts_s >= m_last_ts)
    {
        return std::vector<IPersistentHeavyHitterSketchBITP::HeavyHitter>();
    }

    create_tmp_mg_at(ts_s);

    auto ret = m_tmp_mg->estimate_heavy_hitters(
            frac_threshold - m_epsilon_prime, m_est_tot_cnt);
    
    return ret;
}

void
TreeMisraGriesBITP::create_tmp_mg_at(
    TIMESTAMP ts_s) const
{
    MisraGries *mg = m_cur_sketch->clone();
    uint64_t est_excluded_cnt = 0;
    uint64_t level = m_tree.size();
    while (level > 0)
    {
        if (m_tree[--level]) // level decreased here
        {
            TreeNode *tn = m_tree[level];
            if (tn->m_ts > ts_s)
            {
                // the entire tree is in the range
                ++level;
                break;
            }
            else
            {
                bool does_intersect = false;
                while (tn->m_right)
                {
                    if (tn->m_right->m_ts > ts_s)
                    {
                        // the right subtree is in range
                        does_intersect = true;
                        mg->merge(tn->m_right->m_mg);
                        tn = tn->m_left;
                    }
                    else
                    {
                        // the left subtree is not in range
                        est_excluded_cnt = tn->m_left->m_tot_cnt;
                        tn = tn->m_right;
                    }
                }

                if (does_intersect)
                {
                    break;
                }
                else
                {
                    // the entire tree is not in the range
                    est_excluded_cnt = m_tree[level]->m_tot_cnt;
                }
            }
        }
    }

    while (level > 0)
    {
        TreeNode *tn = m_tree[--level];
        if (tn)
        {
            mg->merge(tn->m_mg);
        }
    }

    m_tmp_ts = ts_s;
    if (m_tmp_mg)
    {
        delete m_tmp_mg;
    }
    m_tmp_mg = mg;
    m_est_tot_cnt = m_tot_cnt - est_excluded_cnt;
}

uint64_t
TreeMisraGriesBITP::estimate_frequency_bitp(
    TIMESTAMP ts_s,
    uint32_t key) const
{
    if (m_tmp_ts != ts_s)
    {
        create_tmp_mg_at(ts_s);
    }

    uint64_t d = (m_epsilon_prime / 2) * m_est_tot_cnt;
    auto &cnt_map = MGA::cnt_map(m_tmp_mg);
    auto iter = cnt_map.find(key);
    if (iter == cnt_map.end()) return 0;
    return iter->second + d;
}

void
TreeMisraGriesBITP::merge_cur_sketch()
{
    TreeNode *tn = new TreeNode;
    tn->m_ts = m_last_ts;
    tn->m_tot_cnt = m_tot_cnt;
    tn->m_mg = m_cur_sketch;
    tn->m_left = nullptr;
    tn->m_right = nullptr;
    tn->m_parent = nullptr;

    m_size_counter += sizeof(TreeNode) + tn->m_mg->memory_usage();
    
    uint32_t level = 0;
    while (m_tree[level])
    {
        TreeNode *tn_merged = new TreeNode;
        tn_merged->m_ts = m_tree[level]->m_ts;
        tn_merged->m_tot_cnt = m_tot_cnt;
        tn_merged->m_left = m_tree[level];
        tn_merged->m_right = tn;
        tn_merged->m_parent = nullptr;
        m_tree[level]->m_parent = tn_merged;
        tn->m_parent = tn_merged;
    
        m_size_counter -= m_tree[level]->m_mg->memory_usage();
        tn_merged->m_mg = m_tree[level]->m_mg;
        m_tree[level]->m_mg = nullptr;
        tn_merged->m_mg->merge(tn->m_mg);
        m_size_counter += sizeof(TreeNode) + tn_merged->m_mg->memory_usage();
        
        tn->m_next = m_right_most_nodes[level]->m_next;
        tn->m_prev = m_right_most_nodes[level];
        m_right_most_nodes[level]->m_next = tn;
        m_right_most_nodes[level] = tn;
        m_tree[level] = nullptr;
        
        if (level < m_level)
        {
            // remove the left most 2 nodes at this level
            TreeNode *n = m_right_most_nodes[level]->m_next;
            TreeNode *n2 = n->m_next;
            TreeNode *p = n->m_parent;
            m_right_most_nodes[level]->m_next = n2->m_next;
    
            assert(p == n2->m_parent);
            assert(!n->m_mg);
            assert(n2->m_mg);
            assert(!n->m_left && !n->m_right);
            assert(!n2->m_left && !n2->m_right);

            m_size_counter -= n2->m_mg->memory_usage() + 2 * sizeof(TreeNode);
            p->m_left = p->m_right = nullptr;
            delete n2->m_mg;
            delete n;
            delete n2;

            // compress p
            if (p->m_mg)
            {
                uint64_t tot_cnt = m_tot_cnt - p->m_prev->m_tot_cnt;
                uint64_t threshold = (uint64_t) std::floor(tot_cnt * m_epsilon_prime);

                m_size_counter -= p->m_mg->memory_usage();
                
                if (MGA::delta(p->m_mg) != 0)
                {
                    MGA::reset_delta(p->m_mg);
                }
                cnt_map_t& cnt_map = MGA::cnt_map(p->m_mg);
                for (auto iter = cnt_map.begin();
                        iter != cnt_map.end();)
                {
                    if (iter->second <= threshold)
                    {
                        iter = cnt_map.erase(iter);
                    }
                    else
                    {
                        ++iter;
                    }
                }

                m_size_counter += p->m_mg->memory_usage();
            }
        }
        
        tn = tn_merged;
        ++level;
        if (level >= m_tree.size())
        {
            throw "Too many items inserted into TMG-BITP"; 
        }
    }

    m_tree[level] = tn;
    if (!m_right_most_nodes[level])
    {
        // first node at this level
        m_right_most_nodes[level] = tn;
        tn->m_next = tn;
        tn->m_prev = nullptr; // prev list is not circular
    }
    else
    {
        tn->m_next = m_right_most_nodes[level]->m_next;
        tn->m_prev = m_right_most_nodes[level];
        m_right_most_nodes[level]->m_next = tn;
        m_right_most_nodes[level] = tn;
    }

    if (level >= m_level)
    {
        if (--m_remaining_nodes_at_cur_level == 0)
        {
            m_remaining_nodes_at_cur_level = m_k;
            ++m_level;
        }
    }

    m_cur_sketch = new MisraGries(m_k);
}

int
TreeMisraGriesBITP::num_configs_defined()
{
    if (g_config->is_list("TMG_BITP.epsilon"))
    {
        int len = (int) g_config->list_length("TMG_BITP.epsilon");
        return len;
    }
    
    return -1;
}

TreeMisraGriesBITP*
TreeMisraGriesBITP::get_test_instance()
{
    return new TreeMisraGriesBITP(0.1);
}

TreeMisraGriesBITP*
TreeMisraGriesBITP::create_from_config(
    int idx)
{
    double epsilon = g_config->get_double("TMG_BITP.epsilon", idx).value();

    return new TreeMisraGriesBITP(epsilon);
}

} // namespace MisraGriesSketches


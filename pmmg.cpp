#include "pmmg.h"
#include "conf.h"
#include <cmath>
#include <numeric>

namespace MisraGriesSketches {

struct MisraGriesAccessor {
    static cnt_map_t&
    cnt_map(MG *mg) { return mg->m_cnt; }

    static void
    reset_delta(MG *mg) { mg->reset_delta(); }
};

using MGA = MisraGriesAccessor;

ChainMisraGries::ChainMisraGries(
    double epsilon):
    m_epsilon(epsilon),
    m_epsilon_over_3(epsilon / 3),
    m_k((uint32_t) std::ceil(1 / m_epsilon_over_3)),
    m_tot_cnt(0u),
    m_last_ts(0u),
    m_cur_sketch(m_k),
    m_snapshot_cnt_map((cnt_map_t::size_type) std::ceil(umap_default_load_factor * (m_k > 1 ? m_k - 1: 1))),
    m_checkpoints(),
    m_delta_list_tail_ptr(nullptr),
    m_num_delta_nodes_since_last_chkpt(0)
{
}

ChainMisraGries::~ChainMisraGries()
{
    clear();
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
}

size_t
ChainMisraGries::memory_usage() const
{
    return 56 + // scalar members
        m_cur_sketch.memory_usage() +
        size_of_unordered_map(m_snapshot_cnt_map) +
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
                return acc;
            });
}

std::string
ChainMisraGries::get_short_description() const
{
    return "CMG-e" + std::to_string(m_epsilon);
}

void
ChainMisraGries::update(
    TIMESTAMP ts,
    uint32_t key,
    int c)
{
    if (m_last_ts > 0 && ts != m_last_ts)
    {
        // ts change, find delta changes and make checkpoint if necessary
        MGA::reset_delta(&m_cur_sketch);
        if (m_checkpoints.empty())
        {
            // first checkpoint
            make_checkpoint();
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
                    m_snapshot_cnt_map.erase(iter_snapshot++);
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
                make_checkpoint();
            }
            else
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
    auto iter = std::upper_bound(
        m_checkpoints.begin(), m_checkpoints.end(),
        ts_e,
        [](TIMESTAMP ts_e, const ChkptNode &n) -> bool {
            return ts_e < n.m_ts;
        });

    if (iter == m_checkpoints.begin())
    {
        return std::vector<HeavyHitter>();
    }

    const ChkptNode &last_chkpt = *(iter - 1);

    std::unordered_map<key_type, uint64_t> snapshot;
    uint64_t d = (uint64_t) std::floor(last_chkpt.m_tot_cnt * m_epsilon_over_3);
    for (const auto &p: last_chkpt.m_cnt_map)
    {
        if (p.second > d)
        {
            snapshot[p.first] = p.second - d;
        }
    }

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
        n = n->m_next;
    }
    
    // Note: m_tot_cnt is an underestimate of m_tot_cnt at timestamp ts_e and it
    // can deviate by an arbitrarily large number
    uint64_t threshold = (uint64_t) (m_epsilon_over_3 + frac_threshold - m_epsilon) * last_chkpt.m_tot_cnt;
    std::vector<HeavyHitter> ret;
    for (auto &p: snapshot)
    {
        if (p.second >= threshold)
        {
            ret.emplace_back(HeavyHitter{
                p.first, (float)((double) p.second / last_chkpt.m_tot_cnt - m_epsilon_over_3)
            });
        }
    }

    return std::move(ret);
}

void
ChainMisraGries::make_checkpoint()
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

    return new ChainMisraGries(epsilon);
}

int
ChainMisraGries::num_configs_defined()
{
    if (g_config->is_list("CMG.epsilon")) 
    {
        int len = (int) g_config->list_length("CMG.epsilon");
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

}

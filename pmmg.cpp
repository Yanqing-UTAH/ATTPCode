#include "pmmg.h"
#include "conf.h"
#include <cmath>
#include <numeric>
#include <cassert>
#include <iostream>

namespace MisraGriesSketches {

struct MisraGriesAccessor {
    static cnt_map_t&
    cnt_map(MG *mg) { return mg->m_cnt; }

    static void
    reset_delta(MG *mg) { mg->reset_delta(); }

    static MG::UpdateResult
    update_impl(
        MG          *mg,
        uint32_t    element,
        int         cnt,
        uint64_t    *p_sub_amount)
    {
        return mg->update_impl(element, cnt, p_sub_amount);
    }
};

using MGA = MisraGriesAccessor;

static MisraGries*
merge_misra_gries(
    MisraGries *mg1,
    MisraGries *mg2);

//
// ChainMisraGries implementation
// 

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
    m_num_delta_nodes_since_last_chkpt(0),
    m_last_chkpt_or_dnode_cnt(0u)
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
    m_last_chkpt_or_dnode_cnt = 0;
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
    TIMESTAMP           ts,
    uint32_t            key,
    int                 c)
{
    update_new(ts, key, c);
}

void
ChainMisraGries::update_new(
    TIMESTAMP           ts,
    uint32_t            key,
    int                 c)
{
    if (m_last_ts > 0 && ts != m_last_ts)
    {
        // TODO check if we have delta changes and make checkpoint if necessary
    }
     
    
    auto update_result = MGA::update_impl(&m_cur_sketch, key, c, &m_sub_amount);
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
        MGA::reset_delta(&m_cur_sketch);
        if (m_checkpoints.empty())
        {
            // first checkpoint
            make_checkpoint();
        }
        else
        {
            //uint64_t last_chkpt_cnt = m_checkpoints.back().m_tot_cnt;
            //uint64_t max_cnt_allowed_before_check =
                //get_allowable_approx_cnt_upper_bound(
                    //m_last_chkpt_or_dnode_cnt, last_chkpt_cnt); 
            //if (m_tot_cnt > max_cnt_allowed_before_check)
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
                else if (new_delta_list)
                {
                    *m_delta_list_tail_ptr = new_delta_list;
                    m_delta_list_tail_ptr = new_delta_list_tail_ptr;
                    m_num_delta_nodes_since_last_chkpt = num_deltas;
                    m_last_chkpt_or_dnode_cnt = m_tot_cnt;
                }
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
    uint64_t tot_cnt_at_ts_e;
    if (prev_ts == next_ts)
    {
        assert(ts_e == prev_ts);
        tot_cnt_at_ts_e = iter->m_tot_cnt;
    }
    else
    {
        tot_cnt_at_ts_e = (
            (next_tot_cnt - prev_tot_cnt) * 1.0 * ts_e +
            prev_tot_cnt * 1.0 * next_ts -
            next_tot_cnt * 1.0 * prev_ts) / (next_ts - prev_ts);
    }

    
    uint64_t threshold = (uint64_t) std::ceil((m_epsilon_over_3 + frac_threshold - m_epsilon) * tot_cnt_at_ts_e);
    std::vector<HeavyHitter> ret;
    for (auto &p: snapshot)
    {
        if (p.second >= threshold)
        {
            ret.emplace_back(HeavyHitter{
                p.first, (float)((double) p.second / tot_cnt_at_ts_e - m_epsilon_over_3)
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

    m_last_chkpt_or_dnode_cnt = m_tot_cnt;
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

//
// TreeMisraGries implementation
//

TreeMisraGries::TreeMisraGries(
    double epsilon):
    m_epsilon(epsilon),
    m_epsilon_prime(epsilon / 2.0),
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
    return 104 + // sclar members
        512 + // size of m_tree = 8 * 64
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
        //std::cout << "TMG " << m_epsilon << " " << ts_e << " " << frac_threshold << std::endl;
        level = m_level;
        mg = new MisraGries(m_k);
        for (; level < m_tree.size(); ++level)
        {
            /*std::cout << level << " ";
            if (!m_tree[level])
            {
                std::cout << "null";
            }
            else
            {
                std::cout << m_tree[level]->m_ts;
            } */
            if (m_tree[level])
            {
                if (m_tree[level]->m_ts >= ts_e)
                {
                    bool does_intersect = false;
                    TreeNode *tn = m_tree[level];
                    while (tn->m_left)
                    {
                        //std::cout << ' ' << tn->m_left->m_ts;
                        if (tn->m_left->m_ts >= ts_e)
                        {
                            //std::cout << " l";
                            tn = tn->m_left;
                        }
                        else
                        {
                            //std::cout << " r";
                            does_intersect = true;
                            mg->merge(tn->m_left->m_mg);
                            est_tot_cnt = tn->m_left->m_tot_cnt;
                            tn = tn->m_right;
                        }
                    }
                    if (does_intersect)
                    {
                        ++level;
                        //std::cout << std::endl;
                        break;
                    }
                }
                else
                {
                    // the entire tree is in range
                    est_tot_cnt = m_tree[level]->m_tot_cnt;
                    //std::cout << std::endl;
                    break;            
                }
            }
            //std::cout << std::endl;
        }
    }

    //std::cout << "Remaining levels:" << std::endl;
    for (; level < m_tree.size(); ++level)
    {
        if (m_tree[level])
        {
            /*std::cout << '\t' << level
                << ' ' << m_tree[level]->m_ts
                << ' ' << MGA::cnt_map(m_tree[level]->m_mg).size()
                << std::endl; */
            assert(ts_e > m_tree[level]->m_ts);
            mg->merge(m_tree[level]->m_mg);
        }
    }
    //std::cout << std::endl;
    
    //std::cout << "est_tot_cnt = " << est_tot_cnt << std::endl;
    //est_tot_cnt = (ts_e > m_tot_cnt) ? m_tot_cnt : (ts_e);
    //std::cout << "est_tot_cnt = " << est_tot_cnt << std::endl;
    
    //std::cout << "frac_threshold' = " << frac_threshold - m_epsilon_prime << std::endl;
    //std::cout << "cnt_map.size() = " << MGA::cnt_map(mg).size() << std::endl;
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
    tn->m_mg = m_cur_sketch;
    tn->m_left = tn->m_right = nullptr;
    m_size_counter += sizeof(TreeNode) + tn->m_mg->memory_usage();
    
    uint32_t level = m_level;
    while (m_tree[level])
    {
        TreeNode *tn_merged = new TreeNode;
        tn_merged->m_ts = m_last_ts;
        tn_merged->m_tot_cnt = m_tot_cnt;
        tn_merged->m_mg = merge_misra_gries(tn->m_mg, m_tree[level]->m_mg);
        tn_merged->m_left = m_tree[level];
        tn_merged->m_right = tn;
        m_size_counter += sizeof(TreeNode) + tn_merged->m_mg->memory_usage();

        m_tree[level] = nullptr;
        tn = tn_merged;
        ++level;

        if (level >= m_tree.size())
        {
            throw "Too many items inserted into TMG"; 
        }
    }

    m_tree[level] = tn;

    if (--m_remaining_nodes_at_cur_level == 0)
    {
        ++m_level;
        //std::cout << "merge_cur_sketch(): " << m_level << ' ' << m_tot_cnt << std::endl;
        m_remaining_nodes_at_cur_level = m_k;
        m_max_cnt_per_node_at_cur_level *= 2;
    }

    m_target_cnt = m_tot_cnt + m_max_cnt_per_node_at_cur_level;
    m_cur_sketch = new MisraGries(m_k);
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

static MisraGries*
merge_misra_gries(
    MisraGries *mg1,
    MisraGries *mg2)
{
    MisraGries *mg_merged = mg1->clone();
    mg_merged->merge(mg2);
    return mg_merged;
}

}

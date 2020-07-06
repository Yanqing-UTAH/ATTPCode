#ifndef PMMG_H
#define PMMG_H

#include <vector>
#include "util.h"
#include "misra_gries.h"
#include "sketch.h"
#include "min_heap.h"

namespace MisraGriesSketches {

typedef uint32_t key_type;
typedef std::unordered_map<key_type, uint64_t> cnt_map_t;
typedef MisraGries MG;

constexpr double umap_default_load_factor = 1.25;

class ChainMisraGries:
    public IPersistentHeavyHitterSketch,
    public IPersistentFrequencyEstimationSketch
{
private:
    static constexpr ptrdiff_t     m_before_first_delta_idx = ~(ptrdiff_t) 0; 
    
    struct DeltaNode;
    struct ChkptNode {
        TIMESTAMP           m_ts;

        uint64_t            m_tot_cnt;
        
        DeltaNode           *m_first_delta_node;

        cnt_map_t           m_cnt_map;
    };
    
    struct DeltaNode {
        TIMESTAMP           m_ts;

        uint64_t            m_tot_cnt;

        key_type            m_key;

        uint64_t            m_new_cnt;

        DeltaNode           *m_next;
    };

    struct SnapshotCounter
    {
        uint64_t                m_cnt;

        uint64_t                m_last_tot_cnt;
    };
    
    // Let c' be the actual counter, c be the counter since the last recorded
    // change (in dnode or checkpoint), and N be the total count when the last
    // recorded change happens. These variables are different for different keys.
    //
    // Any change to c' within c +/- eps/3 * N is not recorded until the threshold
    // is reached. Then the change is recorded at the end of that timestamp.
    //
    // The pending amounts to be subtracted across all counters, which is d =
    // m_sub_amount + m_cur_sketch.m_delta and is shared by all counters, are
    // not applied until the next checkpoint. Thus the value of the these
    // counters c'' = c' + d.
    //
    // We maintain two heaps over the counters.
    //
    // 1. A max heap over c1 = c'' - c - eps/3 * N. At the end of each timestamp,
    // all counters with c1 > d need to be recorded.
    //
    // 2. A min heap over c2 = c'' - c + eps/3 * N. At the end of each timestamp,
    // all counters with c2 < d need to be recorded.
    //
    struct Counter
    {
        uint32_t                m_key;

        bool                    m_in_last_snapshot: 1;

        bool                    m_last_node_is_chkpt: 1;

        int64_t                 m_c1;
        
        uint64_t                m_c1_max_heap_idx;

        int64_t                 m_c2;

        uint64_t                m_c2_min_heap_idx;

        union {
            Counter             *m_next_free_counter;

            DeltaNode           *m_prev_delta_node;

            ChkptNode           *m_prev_chkpt_node;
        };

        // debug only
        
        //uint64_t                m_c;

        //uint64_t                m_c_double_prime;

        //uint64_t                m_prev_N;

        //DeltaNode               *m_prev_delta_node;
    
        //ChkptNode               *m_prev_chkpt_node;
    };

    struct InvertedIndexProxy
    {
        typedef uint64_t value_type;

        bool                    m_updating_c1_max_heap;

        uint64_t&
        operator[](Counter *counter) const
        {
            return m_updating_c1_max_heap ? counter->m_c1_max_heap_idx :
                counter->m_c2_min_heap_idx;
        }
    };

public:
    ChainMisraGries(
        double      epsilon,
        bool        use_update_new = true);

    virtual
    ~ChainMisraGries();

    void
    clear() override;

    size_t
    memory_usage() const override;

    std::string
    get_short_description() const override;

    void
    update(
        TIMESTAMP ts,
        uint32_t key,
        int c = 1) override;

    std::vector<HeavyHitter>
    estimate_heavy_hitters(
        TIMESTAMP ts_e,
        double frac_threshold) const override;

    uint64_t
    estimate_frequency(
        TIMESTAMP ts_e,
        uint32_t key) const;

private:
    void
    clear(
        bool reinit);

    void
    update_new(
        TIMESTAMP ts,
        uint32_t key,
        int c);

    void
    update_old(
        TIMESTAMP ts,
        uint32_t key,
        int c);

    void
    make_checkpoint_old();

    void
    make_checkpoint();

    std::pair<uint64_t, uint64_t>
    get_allowable_cnt_range(
        uint64_t cnt,
        uint64_t last_tot_cnt)
    {
        uint64_t d = (uint64_t) std::floor(last_tot_cnt * m_epsilon_over_3);
        return std::make_pair((d < cnt) ? cnt - d: 0, cnt + d);
    }
    
    uint64_t
    get_allowable_cnt_upper_bound(
        uint64_t cnt,
        uint64_t last_tot_cnt)
    {
        uint64_t d = (uint64_t) std::floor(last_tot_cnt * m_epsilon_over_3);
        return cnt + d;
    }

    std::pair<uint64_t, uint64_t>
    get_allowable_approx_cnt_range(const SnapshotCounter &cnt)
    {
        return get_allowable_cnt_range(cnt.m_cnt, cnt.m_last_tot_cnt);     
    }
    
    static int64_t
    c1_max_heap_key_func(const Counter *counter)
    {
        return counter->m_c1;
    }

    static int64_t
    c2_min_heap_key_func(const Counter *counter)
    {
        return counter->m_c2;
    }

    static Counter*
    heap_idx_func(Counter *counter)
    {
        return counter;
    }

    uint64_t
    tot_cnt_at_last_chkpt() const
    {
        return m_checkpoints.empty() ? 0 : m_checkpoints.back().m_tot_cnt;
    }

    void
    create_tmp_cnt_at(
        TIMESTAMP ts_e) const;

    double                      m_epsilon,

                                m_epsilon_over_3;

    uint32_t                    m_k;

    bool                        m_use_update_new;

    uint64_t                    m_tot_cnt;

    TIMESTAMP                   m_last_ts;

    MG                          m_cur_sketch; // the one up to the current count
    
    // for update_old impl. only
    std::unordered_map<key_type, SnapshotCounter>
                                m_snapshot_cnt_map; // the one up to the last delta/chkpt

    std::vector<ChkptNode>      m_checkpoints;

    DeltaNode                   **m_delta_list_tail_ptr;

    size_t                      m_num_delta_nodes_since_last_chkpt;


    // variables for update_new()
    uint64_t                    m_sub_amount;
    
    Counter                     *m_all_counters;

    Counter                     *m_free_counters;

    std::unordered_map<key_type, Counter*>
                                m_key_to_counter_map;

    std::unordered_map<key_type, Counter*>
                                m_deleted_counters;

    std::vector<Counter*>       m_c1_max_heap;

    std::vector<Counter*>       m_c2_min_heap;

    InvertedIndexProxy          m_inverted_index_proxy;

    // variables for frequency estimation
    mutable TIMESTAMP           m_tmp_cnt_ts;
    
    // estimation are in [-2 * eps/3 * N, eps / 3 * N] of the true value 
    mutable std::unordered_map<key_type, uint64_t>
                                m_tmp_cnt_map;

    mutable uint64_t            m_est_tot_cnt;

public:
    static ChainMisraGries*
    get_test_instance();

    static ChainMisraGries*
    create_from_config(int idx = -1);

    static int
    num_configs_defined();

private:
    static void
    clear_delta_list(DeltaNode *n);
};

class TreeMisraGries:
    public IPersistentHeavyHitterSketch
{
private:
    struct TreeNode {
        TIMESTAMP           m_ts;

        uint64_t            m_tot_cnt;

        MisraGries          *m_mg;

        TreeNode            *m_left,

                            *m_right;
    };
    
public:
    TreeMisraGries(
        double  epsilon);

    virtual
    ~TreeMisraGries();
    
    void
    clear() override;

    size_t
    memory_usage() const override;

    std::string
    get_short_description() const override;

    void
    update(
        TIMESTAMP ts,
        uint32_t value,
        int c) override;

    std::vector<IPersistentHeavyHitterSketch::HeavyHitter>
    estimate_heavy_hitters(
        TIMESTAMP ts_e,
        double frac_threshold) const override;

private:
    void
    merge_cur_sketch();

    static void
    clear_tree(
        TreeNode *root);

    double                  m_epsilon,

                            m_epsilon_prime; // epsilon / 3.0

    uint32_t                m_k;

    TIMESTAMP               m_last_ts;
    
    uint64_t                m_tot_cnt;

    std::vector<TreeNode*>  m_tree;

    uint32_t                m_level;

    uint32_t                m_remaining_nodes_at_cur_level;

    uint64_t                m_target_cnt;

    uint64_t                m_max_cnt_per_node_at_cur_level;

    MisraGries              *m_cur_sketch;

    size_t                  m_size_counter;

public:
    static int
    num_configs_defined();

    static TreeMisraGries*
    get_test_instance();

    static TreeMisraGries*
    create_from_config(int idx);
};

class TreeMisraGriesBITP:
    public IPersistentHeavyHitterSketchBITP,
    public IPersistentFrequencyEstimationSketchBITP
{
private:
    struct TreeNode {
        TIMESTAMP           m_ts;

        uint64_t            m_tot_cnt;

        MisraGries          *m_mg;

        TreeNode            *m_parent, 

                            *m_left,

                            *m_right, // right child
                            
                            *m_next, // right sibling
                                     // TreeNodes are linked as a circular list
                                     // in each level through the m_next field.
                            
                            *m_prev; // left sibling
                                     // TreeNodes are also linked as a **regular**
                                     // list in each level through the m_prev field
    };
    
public:
    TreeMisraGriesBITP(
        double epsilon);

    virtual
    ~TreeMisraGriesBITP();

    void
    clear() override;

    size_t
    memory_usage() const override;

    size_t
    max_memory_usage() const override;

    bool
    max_memory_usage_overriden() const override { return true; }

    std::string
    get_short_description() const override;
    
    void
    update(
        TIMESTAMP ts,
        uint32_t value,
        int c) override;

    std::vector<IPersistentHeavyHitterSketchBITP::HeavyHitter>
    estimate_heavy_hitters_bitp(
        TIMESTAMP ts_s,
        double frac_threshold) const override;

    uint64_t
    estimate_frequency_bitp(
        TIMESTAMP ts_s,
        uint32_t key) const override;

private:
    void
    merge_cur_sketch();

    void
    create_tmp_mg_at(
        TIMESTAMP ts_s) const;

    double                  m_epsilon,

                            m_epsilon_prime; // epsilon / 2.0

    uint32_t                m_k;

    TIMESTAMP               m_last_ts;

    uint64_t                m_tot_cnt;

    std::vector<TreeNode*>  m_tree;

    std::vector<TreeNode*>  m_right_most_nodes;

    uint32_t                m_level; // the lowest incomplete level

    uint32_t                m_remaining_nodes_at_cur_level;

    MisraGries              *m_cur_sketch;

    size_t                  m_size_counter;

    size_t                  m_size_counter_max;
    
    // for frequency estimation
    mutable TIMESTAMP       m_tmp_ts; 

    mutable MisraGries      *m_tmp_mg;

    mutable uint64_t        m_est_tot_cnt;

public:
    static int
    num_configs_defined();

    static TreeMisraGriesBITP*
    get_test_instance();

    static TreeMisraGriesBITP*
    create_from_config(
        int idx);
};

} // namespace MisraGriesSketches

using MisraGriesSketches::ChainMisraGries;
using MisraGriesSketches::TreeMisraGries;
using MisraGriesSketches::TreeMisraGriesBITP;

#endif // PMMG_H


#ifndef PMMG_H
#define PMMG_H

#include <vector>
#include "util.h"
#include "misra_gries.h"
#include "sketch.h"

namespace MisraGriesSketches {

typedef uint32_t key_type;
typedef std::unordered_map<key_type, uint64_t> cnt_map_t;
typedef MisraGries MG;

constexpr double umap_default_load_factor = 1.25;

class ChainMisraGries:
    public IPersistentHeavyHitterSketch
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

public:
    ChainMisraGries(
        double epsilon);

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

private:
    void
    clear(
        bool reinit);

    void
    make_checkpoint();

    std::pair<uint64_t, uint64_t>
    get_allowable_approx_cnt_range(
        uint64_t cnt,
        uint64_t last_tot_cnt)
    {
        uint64_t d = (uint64_t) std::floor(last_tot_cnt * m_epsilon_over_3);
        return std::make_pair(
            (d < cnt) ? cnt - d: 0,
            cnt + (uint64_t) std::floor(last_tot_cnt * m_epsilon_over_3));
    }

    struct SnapshotCounter
    {
        uint64_t                m_cnt;

        uint64_t                m_last_tot_cnt;
    };

    std::pair<uint64_t, uint64_t>
    get_allowable_approx_cnt_range(const SnapshotCounter &cnt)
    {
        return get_allowable_approx_cnt_range(cnt.m_cnt, cnt.m_last_tot_cnt);     
    }

    double                      m_epsilon,

                                m_epsilon_over_3;

    uint32_t                    m_k;

    uint64_t                    m_tot_cnt;

    TIMESTAMP                   m_last_ts;

    MG                          m_cur_sketch; // the one up to the current count

    std::unordered_map<key_type, SnapshotCounter>
                                m_snapshot_cnt_map; // the one up to the last delta/chkpt

    std::vector<ChkptNode>      m_checkpoints;

    DeltaNode                   **m_delta_list_tail_ptr;

    size_t                      m_num_delta_nodes_since_last_chkpt;

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
}

using MisraGriesSketches::ChainMisraGries;

#endif // PMMG_H


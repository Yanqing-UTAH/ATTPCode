#ifndef SAMPLING_H
#define SAMPLING_H

#include "sketch.h"
#include <random>
#include "avl.h"
#include "avl_container.h"

#define MAX_SAMPLE_SIZE 0x7fffffffu

// defined in sampling.cpp
namespace SamplingSketchInternals {
    struct Item;
    struct List;
} // namespace SamplingSketchInternals

class SamplingSketch:
    public AbstractPersistentPointQueryable, // str
    public IPersistentHeavyHitterSketch, // u32
    public IPersistentFrequencyEstimationSketch // u32
{
public:
    SamplingSketch(
        unsigned sample_size,
        unsigned seed = 19950810u,
        bool enable_frequency_estimation = false);

    virtual ~SamplingSketch();

    void
    update(
        TIMESTAMP ts,
        const char *str,
        int c = 1) override;

    void
    update(
        TIMESTAMP ts,
        uint32_t value,
        int c = 1) override;

    void
    clear() override;

    size_t
    memory_usage() const override;

    std::string
    get_short_description() const override;

    double
    estimate_point_at_the_time(
        const char *str,
        unsigned long long ts_e) override;

    std::vector<IPersistentHeavyHitterSketch::HeavyHitter>
    estimate_heavy_hitters(
        TIMESTAMP ts_e,
        double frac_threshold) const override;

    uint64_t
    estimate_frequency(
        TIMESTAMP ts_e,
        uint32_t key) const override;

private:
    bool                m_enable_frequency_estimation;

    unsigned            m_sample_size;

    unsigned long long  m_seen;
    
    SamplingSketchInternals::List
                        *m_reservoir;

    std::mt19937        m_rng;

    TIMESTAMP           m_last_ts;

    mutable TIMESTAMP   m_tmp_cnt_ts;

    mutable std::unordered_map<uint32_t, uint64_t>
                        m_tmp_cnt_map;

    std::vector<std::pair<TIMESTAMP, uint64_t>>
                        m_ts2cnt_map;

public:
    static SamplingSketch*
    create(int &argi, int argc, char *argv[], const char **help_str);

    static SamplingSketch*
    get_test_instance();

    static SamplingSketch*
    create_from_config(int idx = -1);

    static int
    num_configs_defined();
};

class SamplingSketchBITP:
    public IPersistentHeavyHitterSketchBITP,
    public IPersistentFrequencyEstimationSketchBITP
{
private:
    struct MinWeightListNode
    {
        double              m_weight;

        MinWeightListNode
                            *m_next;
    };

    struct Item
    {
        TIMESTAMP           m_ts;

        uint32_t            m_value;
        
        MinWeightListNode   m_min_weight_list;

        double              m_my_weight; // TODO can we somehow drop this field?

        // XXX this assumes 8-byte pointers and 4-byte ints
        // XXX and the alignment requirement for pointers and ints are 8 and 4
        uint64_t            : 0;

        char                m_payload[sizeof(Item*) * 6 + sizeof(int) * 2];
    };

    static const dsimpl::AVLNodeDescByOffset<Item, TIMESTAMP>
                            m_ts_map_node_desc;

    static const dsimpl::AVLNodeDescByOffset<Item, double>
                            m_weight_map_node_desc;

    // structs for new impl
    struct ItemNew
    {
        TIMESTAMP           m_ts;

        uint32_t            m_value;

        int                 m_tree_hdiff;   // for m_ts_map_new

        int                 m_tree_hdiff2;  // for m_min_weight_map
        
        uint32_t            m_cnt_field;    // subtree count when in
                                            // m_recent_items_weight_map and
                                            // the number of smaller weights
                                            // that preceed this item when in
                                            // m_ts_map_new
        
        uint64_t            m_seq_no;       // Unfortunately, we need this
                                            // field to differentiate among the
                                            // items with duplicate weights for
                                            // the sample trees inside each
                                            // node


        double              m_weight;

        double              m_min_weight;

        ItemNew             *m_tree_left,

                            *m_tree_right,

                            *m_tree_parent,

                            *m_tree_left2,
                            
                            *m_tree_right2,

                            *m_tree_parent2;

        uint64_t            m_min_heap_idx;

        char                m_payload[0];

        dsimpl::avl_container<
            ItemNew*,
            double (*)(ItemNew*)>
                            *m_samples; // subtree samples
    };

    static constexpr double
    itemnew_weight_key_func(
        ItemNew             *item)
    {
        return item->m_weight; 
    }

    static constexpr double
    itemnew_min_weight_key_func(
        ItemNew             *item)
    {
        return item->m_min_weight;
    }

    static constexpr const ItemNew*
    itemnew_id_key_func(
        ItemNew             *item)
    {
        return item;
    }

    static constexpr bool
    itemnew_total_order_comp_func(
        const ItemNew       *i1,
        const ItemNew       *i2)
    {
        if (i1->m_weight < i2->m_weight) return true;
        if (i1->m_weight > i2->m_weight) return false;
        return i1->m_seq_no > i2->m_seq_no;
    }

    struct ItemNewNodeDesc:
        public dsimpl::AVLNodeDescByOffset<ItemNew, TIMESTAMP>
    {
        ItemNewNodeDesc(
            uint32_t        sample_size);

        void
        _fix_agg(
            ItemNew         *item,
            void            *info,
            dsimpl::Type2AggregateOps
                            op);
        
        uint32_t            m_sample_size;

        uint64_t            m_tot_num_samples;

    private:
        typedef decltype(((ItemNew*) nullptr)->m_samples->begin()) sample_iterator;
        static sample_iterator
        find_smallest_item(
            ItemNew *root); 

        void
        reconstruct_samples_one_side(
            ItemNew         *root,
            ItemNew         *subtree);

        void
        reconstruct_samples_two_sides(
            ItemNew         *root);
    };
    
    // for m_recent_items_weight_map in new impl
    struct ItemNewNodeDesc2:
        public dsimpl::AVLNodeDescByOffset<ItemNew, double> {
        
        ItemNewNodeDesc2();

        void _fix_agg(ItemNew *item) const;
    };
    
    // for m_min_weight_map in new impl
    static const dsimpl::AVLNodeDescByOffset<ItemNew, double>
                            m_itemnew_node_desc3;

    struct Item3
    {
        TIMESTAMP           m_ts;

        uint32_t            m_value;

        double              m_weight;

        Item3               *m_next;
    };

public:
    SamplingSketchBITP(
        uint32_t            sample_size,
        uint32_t            seed = 19950810u,
        uint8_t             use_new_impl = 0,
        bool                enable_frequency_estimation = false);

    ~SamplingSketchBITP();
    
    void
    clear() override;

    size_t
    memory_usage() const override;

    size_t
    max_memory_usage() const override;

    bool
    max_memory_usage_overriden() const { return true; }

    std::string
    get_short_description() const override;
    
    void
    update(
        TIMESTAMP           ts,
        uint32_t            value,
        int                 c = 1) override;

    std::vector<HeavyHitter>
    estimate_heavy_hitters_bitp(
        TIMESTAMP           ts_s,
        double              frac_threshold) const override;

    uint64_t
    estimate_frequency_bitp(
        TIMESTAMP           ts_s,
        uint32_t            key) const override;

private:

    void
    update_old(
        TIMESTAMP           ts,
        uint32_t            value,
        int                 c);

    void
    update_new(
        TIMESTAMP           ts,
        uint32_t            value,
        int                 c);

    void
    update_batched(
        TIMESTAMP           ts,
        uint32_t            value,
        int                 c);

    struct SampleSet {
        uint32_t            m_cur_size;

        uint32_t            m_sample_size;

        ItemNew             *m_pending_combined;

        ItemNew             *m_min_heap[0];
    };

    SampleSet*
    find_sample_set(
        ItemNew             *item) const;
    
    static void
    find_sample_set_combine(
        SampleSet           *sset,
        ItemNew             *item);

    static void
    find_sample_set_exclude(
        SampleSet           *sset,
        ItemNew             *item);

    template<bool has_nonnull_excluded>
    static void
    find_sample_set_exclude_impl(
        SampleSet           *sset,
        ItemNew             *excluded);

    static void
    destroy_sample_set(
        SampleSet           *sset);

    void
    recompute_min_weight(
        ItemNew             *item,
        ItemNew             *inserted_item) const;

    std::vector<HeavyHitter>
    estimate_heavy_hitters_bitp_old(
        TIMESTAMP           ts_s,
        double              frac_threshold) const;

    std::vector<HeavyHitter>
    estimate_heavy_hitters_bitp_new(
        TIMESTAMP           ts_s,
        double              frac_threshold) const;

    std::vector<HeavyHitter>
    estimate_heavy_hitters_bitp_batched(
        TIMESTAMP           ts_s,
        double              frac_threshold) const; 
    
    Item*&
    ith_most_recent_item(ptrdiff_t i) const
    {
        ptrdiff_t i2;
        if (m_most_recent_items_start < i)
        {
            i2 = m_most_recent_items_start + m_sample_size - i;
        }
        else
        {
            i2 = m_most_recent_items_start - i;
        }
        return m_most_recent_items[i2];
    }

    struct ItemNewMinHeapInvertedIndexProxy
    {
        uint64_t&
        operator[](
            ItemNew         *item) const
        {
            return item->m_min_heap_idx;
        }
    };

    static inline constexpr ItemNew*
    itemnew_min_heap_idx_func(
        ItemNew             *item)
    {
        return item;
    }

    uint32_t                m_sample_size;

    uint8_t                 m_use_new_impl;

    bool                    m_enable_frequency_estimation;

    // use_new_impl == 0 (old impl)

    dsimpl::avl_t<decltype(m_ts_map_node_desc)>
                            m_ts_map;

    dsimpl::avl_t<decltype(m_weight_map_node_desc)>
                            m_min_weight_map;
    
    dsimpl::avl_t<decltype(m_weight_map_node_desc)>
                            m_most_recent_items_weight_map;

    Item                    **m_most_recent_items;

    ptrdiff_t               m_most_recent_items_start;

    std::mt19937            m_rng;

    std::uniform_real_distribution<double>
                            m_unif_0_1;

    uint64_t                m_num_items_alloced;

    uint64_t                m_num_mwlistnodes_alloced;

    // use_new_impl == 1 (new impl)
    
    ItemNew                 **m_recent_items_new;

    ptrdiff_t               m_recent_items_start_new;
    
    dsimpl::avl_t<ItemNewNodeDesc, false>
                            m_ts_map_new;
    
    dsimpl::avl_t<ItemNewNodeDesc2, false>
                            m_recent_items_weight_map;
    
    dsimpl::avl_t<decltype(m_itemnew_node_desc3), false>
                            m_min_weight_map_new;

    uint64_t                m_next_seq_no;
     
    uint64_t                m_num_itemnew_alloced;

    // use_new_impl == 2 (batched impl)
    Item3                   *m_item3_head;

    uint64_t                m_num_item3_alloced;

    uint64_t                m_num_item3_alloced_target;

    uint64_t                m_tot_seen;

    uint64_t                m_max_num_item3_alloced;
    
    // frequency estimation
    TIMESTAMP           m_last_ts;

    mutable TIMESTAMP       m_tmp_cnt_ts;

    mutable std::unordered_map<uint32_t, uint64_t>
                            m_tmp_cnt_map;

    std::vector<std::pair<TIMESTAMP, uint64_t>>
                        m_ts2cnt_map;

public:
    static SamplingSketchBITP*
    get_test_instance();

    static SamplingSketchBITP*
    create_from_config(int idx = -1);

    static int
    num_configs_defined();

    static void
    print_tree(
        decltype(m_ts_map_new) *avl);
};

#endif // SAMPLING_H


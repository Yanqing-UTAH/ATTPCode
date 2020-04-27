#ifndef SAMPLING_H
#define SAMPLING_H

#include "sketch.h"
#include <random>
#include "avl.h"

#define MAX_SAMPLE_SIZE 0x7fffffffu

// defined in sampling.cpp
namespace SamplingSketchInternals {
    struct Item;
    struct List;
} // namespace SamplingSketchInternals

class SamplingSketch:
    public AbstractPersistentPointQueryable, // str
    public IPersistentHeavyHitterSketch // u32
{
public:
    SamplingSketch(
        unsigned sample_size,
        unsigned seed = 19950810u);

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

private:
    unsigned            m_sample_size;

    unsigned long long  m_seen;
    
    SamplingSketchInternals::List
                        *m_reservoir;

    std::mt19937        m_rng;

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
    public IPersistentHeavyHitterSketchBITP
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

public:
    SamplingSketchBITP(
        uint32_t            sample_size,
        uint32_t            seed = 19950810u);

    ~SamplingSketchBITP();
    
    void
    clear() override;

    size_t
    memory_usage() const override;

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

private:
    
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

    uint32_t                m_sample_size;
    
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

public:
    static SamplingSketchBITP*
    get_test_instance();

    static SamplingSketchBITP*
    create_from_config(int idx = -1);

    static int
    num_configs_defined();
};

#endif // SAMPLING_H


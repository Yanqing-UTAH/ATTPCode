#ifndef NORM_SAMPLING_H
#define NORM_SAMPLING_H

#include "sketch.h"
#include <unordered_map>
#include <random>

class NormSamplingSketch:
    public IPersistentMatrixSketch
{
private:
    struct Item
    {
        TIMESTAMP               m_ts;

        double                  *m_dvec;

        double                  m_threshold;
    };

    struct List
    {
    public:
        List();

        ~List();

        void
        reset();

        void
        append(
            TIMESTAMP ts,
            double *dvec,
            double weight);

        size_t
        memory_usage() const;

        Item*
        last_of(
            TIMESTAMP ts) const;

        double
        get_weight() const { return m_weight; }

    private:
        void ensure_item_capacity(uint32_t desired_length);

        uint32_t                m_length,
                                
                                m_capacity;

        Item                    *m_items;

        double                  m_weight;
    };


public:
    NormSamplingSketch(
        uint32_t n,
        uint32_t sample_size,
        uint32_t seed = 19950810u);

    virtual
    ~NormSamplingSketch();

    void
    clear() override;

    size_t
    memory_usage() const override;

    std::string
    get_short_description() const override;
    
    void
    update(
        TIMESTAMP ts,
        const double *dvec) override;

    void
    get_covariance_matrix(
        TIMESTAMP ts_e,
        double *A) const override;
    
private:
    int                         m_n;
    
    uint32_t                    m_sample_size;

    uint32_t                    m_seen;

    uint64_t                    m_n_dvec_stored;

    List                        *m_reservoir;
    
    List                        **m_weight_min_heap;

    std::mt19937                m_rng;

    std::uniform_real_distribution<double>
                                m_unif_m1_0; // [-1, 0)

/*    std::vector<std::pair<TIMESTAMP, uint64_t>>
                                m_ts_2_cnt; */

public:
    static NormSamplingSketch*
    get_test_instance();

    static NormSamplingSketch*
    create_from_config(int idx = -1);

    static int
    num_configs_defined();
};

#endif // NORM_SAMPLING_H

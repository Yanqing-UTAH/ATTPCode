#ifndef NORM_SAMPLING_WR_H
#define NORM_SAMPLING_WR_H

#include "sketch.h"
#include <random>
#include <map>

class NormSamplingWRSketch:
    public IPersistentMatrixSketch
{
private:
    struct Item
    {
        TIMESTAMP               m_ts;
    
        // flip bit 63 to indicate ownership
        double                  *m_dvec;
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
            bool is_owner);

        size_t
        memory_usage() const;
    
        // transparently return the latest dvec up to timestamp ts
        double*
        dvec_last_of(
            TIMESTAMP ts) const;

    private:
        void ensure_item_capacity(uint32_t desired_length);

        uint32_t                m_length,
                                
                                m_capacity;

        Item                    *m_items;
    };


public:
    NormSamplingWRSketch(
        uint32_t n,
        uint32_t sample_size,
        uint32_t seed = 19950810u);

    virtual
    ~NormSamplingWRSketch();

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

    TIMESTAMP                   m_last_ts;

    uint64_t                    m_n_dvec_stored;

    double                      m_tot_weight;

    List                        *m_reservoir;
    
    std::mt19937                m_rng;

    std::uniform_real_distribution<double>
                                m_unif_0_1;

    std::map<TIMESTAMP, double> m_sqr_fnorms;

public:
    static NormSamplingWRSketch*
    get_test_instance();

    static NormSamplingWRSketch*
    create_from_config(int idx = -1);

    static int
    num_configs_defined();
};

#endif // NORM_SAMPLING_WR_H

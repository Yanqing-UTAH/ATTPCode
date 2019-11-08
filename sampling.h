#ifndef SAMPLING_H
#define SAMPLING_H

#include "sketch.h"
#include <random>

#define MAX_SAMPLE_SIZE 0x7fffffffu

class SamplingSketch:
    public AbstractPersistentPointQueryable, // str
    public IPersistentHeavyHitterSketch // u32
{
    struct Item {
        unsigned long long  m_ts;
        union {
            ptrdiff_t           m_str_off;
            uint32_t            m_u32;
        }                   m_value;
    }
    ;
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
    
    List                *m_reservoir;

    std::mt19937        m_rng;

public:
    static SamplingSketch*
    create(int &argi, int argc, char *argv[], const char **help_str);

    static SamplingSketch*
    get_test_instance();
};

#endif // SAMPLING_H


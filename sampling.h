#ifndef SAMPLING_H
#define SAMPLING_H

#include "sketch.h"
#include <random>

#define MAX_SAMPLE_SIZE 0x7fffffffu

class SamplingSketch:
public AbstractPersistentPointQueryable {
    
    struct Item {
        unsigned long long  m_ts;
        ptrdiff_t           m_value_off;
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
            unsigned long long ts,
            const char *str);

        size_t
        memory_usage() const;

        const char * 
        last_of(unsigned long long ts) const;

    private: 
        // 32 bits should be enough for length which grows in logarithm
        unsigned            m_length,

                            m_capacity;

        Item                *m_items;

        unsigned            m_end_of_content,

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
        unsigned long long ts,
        const char *str,
        int c = 1) override;

    void
    clear() override;

    size_t
    memory_usage() override;

    double
    estimate_point_at_the_time(
        const char *str,
        unsigned long long ts_e) override;

private:
    unsigned            m_sample_size;

    unsigned long long  m_seen;
    
    List                *m_reservoir;

    std::mt19937        m_rng;

public:
    static SamplingSketch*
    create(int argc, char *argv[], const char **help_str);
};

#endif // SAMPLING_H


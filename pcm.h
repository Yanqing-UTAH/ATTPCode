#ifndef pcm_h
#define pcm_h

#include <cmath>
#include <cstring>
#include <limits>
#include <algorithm>
#include <vector>
#include "pla.h"
#include "util.h"
#include "sketch.h"
#include "MurmurHash3.h"

class CMSketch {
    protected:
        const unsigned int w, d;
        std::vector<std::vector<int>> C;
        std::vector<std::pair<uint64_t, uint64_t>> m_u32_hash_param;

    public:
        CMSketch(double eps, double delta, uint64_t seed = 19950810u);

        void clear();

        void update(const char *, int = 1);

        uint64_t estimate(const char *) const;

        size_t memory_usage() const;
    
    protected:
        void update_impl(uint64_t hashval, int c);
        
        uint64_t estimate_impl(uint64_t hashval) const;

        unsigned int u32_hash(unsigned j, uint64_t key) const
        {
            auto x = m_u32_hash_param[j].first * key + m_u32_hash_param[j].second;
            return (unsigned)(x % w);
        }
};

class PCMSketch :
    protected CMSketch,
    public AbstractPersistentPointQueryable, // str
    public IPersistentFrequencyEstimationSketch, // u32
    public IPersistentFrequencyEstimationSketchBITP // u32 
{
    private:
        std::vector<std::vector<PLA>> pla;

        double m_eps;
        double m_delta;
        double m_Delta;

    public:
        PCMSketch(double eps, double delta, double Delta, uint64_t seed = 19950810ul);

        void clear() override;
        
        void update(unsigned long long ts, const char *str, int c = 1) override;

        void
        update(
            TIMESTAMP ts,
            uint32_t key,
            int c = 1) override;

        double
        estimate_point_in_interval(
            const char *str,
            unsigned long long ts_s,
            unsigned long long ts_e) override;

        double
        estimate_point_at_the_time(
            const char *str,
            unsigned long long ts_e) override;     

        uint64_t
        estimate_frequency(
            TIMESTAMP ts_e,
            uint32_t key) const override;

        uint64_t
        estimate_frequency_bitp(
            TIMESTAMP ts_s,
            uint32_t key) const override;

        size_t memory_usage() const override;

        std::string get_short_description() const override;

    private:
        void
        update_impl(
            TIMESTAMP ts,
            uint64_t hashval,
            int c);
        
        double
        estimate_point_in_interval_impl(
            uint64_t hashval,
            TIMESTAMP ts_s,
            TIMESTAMP ts_e) const;

    public:
        static PCMSketch *create(int &argi, int argc, char *argv[], const char **help_str);

        static PCMSketch *get_test_instance();

        static PCMSketch *create_from_config(int idx = -1);
};

#endif

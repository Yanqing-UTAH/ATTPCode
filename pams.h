#ifndef pams_h
#define pams_h

#include <cmath>
#include <cstring>
#include <limits>
#include <algorithm>
#include <vector>
#include <random>
#include <array>
#include <tuple>
#include "util.h"
#include "sketch.h"
#include "MurmurHash3.h"


class PAMSketch:
    public AbstractPersistentPointQueryable, // str
    public IPersistentFrequencyEstimationSketch, // u32
    public IPersistentFrequencyEstimationSketchBITP // u32
    {
    protected:
        struct Counter {
            Counter(): val(0), samples() {}

            int val;
            std::vector<std::pair<unsigned long long, int>> samples;
        };
    
        const unsigned int w, d, D;
        std::vector<std::vector<std::array<Counter, 2>>> C;

        // need 4-wise independent hash rather than 2-wise
        std::vector<std::tuple<int, int, int, int>> ksi;

        std::bernoulli_distribution p_sampling;

        std::mt19937 rgen;

        double m_eps;
        double m_delta;
        double m_Delta;
    
        std::vector<std::pair<uint64_t, uint64_t>> m_u32_hash_param;
    
    public:
        PAMSketch(double eps, double delta, double Delta,
                uint32_t seed = 19950810u);

        void clear() override;

        void update(unsigned long long t, const char *str, int c = 1) override;

        void
        update(
            TIMESTAMP ts,
            uint32_t value,
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

    protected:
        void update_impl(TIMESTAMP ts, uint64_t hashval, int c);

        double estimate_point_in_interval_impl(
            uint64_t hashval,
            TIMESTAMP ts_s,
            TIMESTAMP ts_e) const;
    
        // use j^th hash to hash value i
        //inline unsigned int h(unsigned j, const void *dat, size_t len) {
        //    uint64_t h[2];
        //    MurmurHash3_x64_128(dat, len, j, h);
        //    return (h[0] ^ h[1]) % w;
        //}

        // j^th ksi to hash value i (mapped to 0, 1)
        inline unsigned int flag(unsigned j, unsigned int i) const {
            return (((std::get<0>(ksi[j]) * i + std::get<1>(ksi[j]))
                * i + std::get<2>(ksi[j])) * i + std::get<3>(ksi[j])) % 2;
        }

        inline int Xi(unsigned j,  unsigned i) const {
            return (int)(flag(j, i) * 2) - 1;
        }

        double estimate_C(unsigned j, unsigned i, unsigned int f, unsigned long long t) const;

        void prepare_u32_hash();

        unsigned int u32_hash(unsigned j, uint64_t key) const
        {
            auto x = m_u32_hash_param[j].first * key + m_u32_hash_param[j].second;
            return (unsigned)(x % w);
        }

    public:
        static PAMSketch *create(int &argi, int argc, char *argv[], const char **help_str);

        static PAMSketch *get_test_instance();
    
        static PAMSketch *create_from_config(int idx = -1);
};

#endif // PAMS_H


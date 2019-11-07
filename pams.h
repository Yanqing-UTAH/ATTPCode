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


class PAMSketch: public AbstractPersistentPointQueryable {
    protected:
        struct Counter {
            Counter(): val(0), samples() {}

            int val;
            std::vector<std::pair<unsigned long long, int>> samples;
        };

        const unsigned int w, d, D;
        std::vector<std::vector<std::array<Counter, 2>>> C;
        std::vector<std::pair<int, int>> hashes;

        // need 4-wise independent hash rather than 2-wise
        std::vector<std::tuple<int, int, int, int>> ksi;

        std::bernoulli_distribution p_sampling;

        std::mt19937 rgen;
    
    public:
        PAMSketch(double eps, double delta, double Delta);

        void clear() override;

        void update(unsigned long long t, const char *str, int c = 1) override;

        double
        estimate_point_in_interval(
            const char *str,
            unsigned long long ts_s,
            unsigned long long ts_e) override;

        double
        estimate_point_at_the_time(
            const char *str,
            unsigned long long ts_e) override;

        size_t memory_usage() const override;

    protected:
    
        // use j^th hash to hash value i
        inline unsigned int h(unsigned j, unsigned int i) {
            return (hashes[j].first * i + hashes[j].second) % w;
        }
    
        // j^th ksi to hash value i (mapped to 0, 1)
        inline unsigned int flag(unsigned j, unsigned int i) {
            return (((std::get<0>(ksi[j]) * i + std::get<1>(ksi[j]))
                * i + std::get<2>(ksi[j])) * i + std::get<3>(ksi[j])) % 2;
        }

        inline int Xi(unsigned j,  unsigned i) {
            return (int)(flag(j, i) * 2) - 1;
        }

        double estimate_C(unsigned j, unsigned i, unsigned int f, unsigned long long t);

    public:
        static PAMSketch *create(int &argi, int argc, char *argv[], const char **help_str);

        static PAMSketch *get_test_instance();
};

#endif // PAMS_H


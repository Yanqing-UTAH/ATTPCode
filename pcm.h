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
        std::vector<int> hashes;

    public:
        CMSketch(double, double);

        void clear();

        void update(const char *, int = 1);

        int estimate(const char *);

        unsigned long long memory_usage() const;
};

class PCMSketch :
    protected CMSketch,
    public AbstractPersistentPointQueryable {

    private:
        std::vector<std::vector<PLA>> pla;

        double m_eps;
        double m_delta;
        double m_Delta;
        
    public:
        PCMSketch(double eps, double delta, double Delta);

        void clear() override;
        
        void update(unsigned long long ts, const char *str, int c = 1) override;

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

        std::string get_short_description() const override;

    public:
        static PCMSketch *create(int &argi, int argc, char *argv[], const char **help_str);

        static PCMSketch *get_test_instance();

        static PCMSketch *create_from_config(int idx = -1);
};

#endif

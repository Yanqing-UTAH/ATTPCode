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

class CMSketch {
    protected:
        const unsigned int w, d;
        std::vector<std::vector<int>> C;
        std::vector<std::pair<int, int>> hashes;

    public:
        CMSketch(double, double);

        void clear();

        void update(const char *, int = 1);

        int estimate(const char *);

        unsigned long long memory_usage();
};

class PCMSketch :
    protected CMSketch,
    public AbstractPersistentPointQueryable {

    private:
        std::vector<std::vector<PLA>> pla;
        
    public:
        PCMSketch(double eps, double delta, double Delta);

        void clear() override;
        
        void update(unsigned long long ts, const char *str, int c = 1) override;

        double
        estimate_point_in_interval(
            const char * str,
            unsigned long long ts_s,
            unsigned long long ts_e) override;

        size_t memory_usage() override;

    public:
        static PCMSketch *create(int argc, char *argv[], const char **help_str);
};

#endif

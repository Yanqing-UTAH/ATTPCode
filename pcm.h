#ifndef pcm_h
#define pcm_h

#include <cmath>
#include <cstring>
#include <limits>
#include <algorithm>
#include <vector>
#include "pla.h"
#include "util.h"

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

class PCMSketch : public CMSketch {
    private:
        std::vector<std::vector<PLA>> pla;
        
    public:
        PCMSketch(double, double, double);

        void clear();
        
        void update(unsigned long long, const char *, int = 1);

        double estimate(const char *, unsigned long long, unsigned long long);

        unsigned long long memory_usage();
};

#endif

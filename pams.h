#ifndef pams_h
#define pams_h

#include <cmath>
#include <cstring>
#include <limits>
#include <algorithm>
#include <vector>
#include "util.h"

class PAMS {
    protected:
        const unsigned int w, d;
        std::vector<std::vector<std::array<int, 2>>> C;
        std::vector<std::pair<int, int>> hashes;
        std::vector<std::pair<int, int>> flags;

    public:
        PAMS(double, double);

        void clear();

        void update(const char *, int = 1);

        int estimate(const char *);

        size_t memory_usage();
};

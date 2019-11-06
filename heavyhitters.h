#ifndef HEAVYHITTERS_H
#define HEAVYHITTERS_H

#include <vector>
#include "pcm.h"

class HeavyHitters {

    public:
        HeavyHitters(unsigned logUniverseSize);

        ~HeavyHitters();

        void update(unsigned long long ts, unsigned element, int cnt = 1);

	std::vector<unsigned long long> query_hh(unsigned long long ts, double threshold) const;

	size_t memory_usage() const;

    private:
        int levels;
        PCMSketch **pcm;
};


#endif

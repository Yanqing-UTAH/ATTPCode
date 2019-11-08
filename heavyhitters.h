#ifndef HEAVYHITTERS_H
#define HEAVYHITTERS_H

#include <vector>
#include "pcm.h"

class HeavyHitters:
    public IPersistentHeavyHitterSketch {

public:
    HeavyHitters(unsigned logUniverseSize);

    HeavyHitters(
        unsigned logUniverseSize,
        double epsilon,
        double delta,
        double Delta);

    virtual ~HeavyHitters();

    void clear() override;

    void update(unsigned long long ts, uint32_t element, int cnt = 1) override;
    
    /* absolute threshold */
	std::vector<uint32_t> query_hh(unsigned long long ts, double threshold) const;

    std::vector<IPersistentHeavyHitterSketch::HeavyHitter>
    estimate_heavy_hitters(
        TIMESTAMP ts_e,
        double frac_threshold) const override;

	size_t memory_usage() const override;

    private:
        int levels;
        
        PCMSketch **pcm;
        
        uint64_t tot_cnt; 

        PLA *cnt_pla;

    public:
        static HeavyHitters* create(int &argi, int argc, char *argv[], const char **help_str);

        static HeavyHitters* get_test_instance();
};


#endif

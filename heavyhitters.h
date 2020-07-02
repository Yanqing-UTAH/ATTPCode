#ifndef HEAVYHITTERS_H
#define HEAVYHITTERS_H

#include <vector>
#include "pcm.h"

class HeavyHitters:
    public IPersistentHeavyHitterSketch,
    public IPersistentHeavyHitterSketchBITP {

public:
    HeavyHitters(unsigned logUniverseSize);

    HeavyHitters(
        unsigned logUniverseSize,
        double epsilon,
        double delta,
        double Delta,
        uint64_t seed = 19950810ul);

    virtual ~HeavyHitters();

    void clear() override;

    void update(unsigned long long ts, uint32_t element, int cnt = 1) override;
    
    /* absolute threshold */
	std::vector<uint32_t> query_hh(unsigned long long ts, double threshold) const;

    std::vector<uint32_t> query_hh_bitp(unsigned long long ts, double threshold) const;

    std::vector<IPersistentHeavyHitterSketch::HeavyHitter>
    estimate_heavy_hitters(
        TIMESTAMP ts_e,
        double frac_threshold) const override;

    std::vector<IPersistentHeavyHitterSketchBITP::HeavyHitter>
    estimate_heavy_hitters_bitp(
        TIMESTAMP ts_s,
        double frac_threshold) const override;

	size_t memory_usage() const override;

    std::string get_short_description() const override;

    private:
        int levels;
        
        PCMSketch **pcm;
        
        uint64_t tot_cnt; 

        PLA *cnt_pla;

        double m_eps;
        double m_delta;
        double m_Delta;

    public:
        static HeavyHitters* create(int &argi, int argc, char *argv[], const char **help_str);

        static HeavyHitters* get_test_instance();

        static HeavyHitters* create_from_config(int idx = -1);

        static int num_configs_defined();
};


#endif

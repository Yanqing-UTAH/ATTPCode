#include "sketch.h"
#include <unordered_map>

class ExactHeavyHitters:
    public IPersistentHeavyHitterSketch
{
private:
    struct Item
    {
        TIMESTAMP       m_ts;

        uint64_t        m_cnt;
    };

public:
    ExactHeavyHitters();
    
    virtual ~ExactHeavyHitters();

    void
    clear() override;

    size_t
    memory_usage() const override;

    void
    update(TIMESTAMP ts, uint32_t value, int c = 1) override;

    std::vector<IPersistentHeavyHitterSketch::HeavyHitter>
    estimate_heavy_hitters(
        TIMESTAMP ts_e,
        double frac_threshold) const override;

private:
    std::unordered_map<uint32_t, std::vector<Item>> m_items;

    std::unordered_map<uint32_t, std::vector<Item>>::size_type
                                                    m_initial_bucket_count;

public:

    static ExactHeavyHitters *create(int &argi, int argc, char *argv[], const char **help_str);

    static ExactHeavyHitters *get_test_instance();
};


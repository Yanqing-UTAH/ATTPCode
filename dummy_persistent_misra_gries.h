#ifndef DUMMY_PERSISTENT_MISRA_GRIES_H
#define DUMMY_PERSISTENT_MISRA_GRIES_H

#include "misra_gries.h"
#include "conf.h"


// 
// DummyPersistentMisraGries exposes an IPersistentHeavyHitterSketch
// interface but ignores any timestamp arguments passed to it.
//
class DummyPersistentMisraGries:
    public IPersistentHeavyHitterSketch,
    public MisraGries
{
public:
    DummyPersistentMisraGries(
        double epsilon):
        MisraGries(epsilon),
        m_tot_cnt(0)
    {}

    size_t
    memory_usage() const override
    {
        return MisraGries::memory_usage() + sizeof(m_tot_cnt);
    }

    void
    clear()
    {
        MisraGries::clear();
        m_tot_cnt = 0;
    }

    std::string
    get_short_description() const override
    {
        return "DummyPersistentMisraGries-e" + std::to_string(get_eps());
    }

    void
    update(
        TIMESTAMP ts,
        uint32_t value,
        int c = 1) override
    {
        MisraGries::update(value, c);
        m_tot_cnt += c;
    }

    std::vector<HeavyHitter>
    estimate_heavy_hitters(
        TIMESTAMP ts_e,
        double frac_threshold) const override
    {
        return MisraGries::estimate_heavy_hitters(frac_threshold, m_tot_cnt);
    }

private:
    uint64_t            m_tot_cnt;

public:
    static DummyPersistentMisraGries*
    get_test_instance()
    {
        return new DummyPersistentMisraGries(0.1);
    }

    static DummyPersistentMisraGries*
    create_from_config(
        int idx = -1)
    {
        double epsilon = g_config->get_double("DUMMY_PMG.epsilon", idx).value();

        return new DummyPersistentMisraGries(epsilon);
    }

    static int
    num_configs_defined()
    {
        if (g_config->is_list("DUMMY_PMG.epsilon"))
        {
            int len = (int) g_config->list_length("DUMMY_PMG.epsilon");
            return len;
        }

        return -1;
    }
};

#endif // DUMMY_PERSISTENT_MISRA_GRIES_H

#ifndef MISRA_GRIES_H
#define MISRA_GRIES_H

#include <unordered_map>
#include <list>
#include "hashtable.h"
#include "sketch.h"

namespace MisraGriesSketches {
    class MisraGriesAccessor;
}

class MisraGries
{
public:
    MisraGries(
        double epsilon);

    MisraGries(
        uint32_t k);

    ~MisraGries();

    MisraGries(const MisraGries&) = default;
    MisraGries(MisraGries&&) = default;
    MisraGries& operator=(const MisraGries&) = default;
    MisraGries& operator=(MisraGries&&) = default;

    void
    clear();

    double
    get_eps() const
    {
        return m_eps;
    }

    size_t
    memory_usage() const;

    void
    update(
        uint32_t element,
        int cnt = 1);

    MisraGries*
    clone();

    void
    merge(
        MisraGries *mg2);

    std::vector<HeavyHitter_u32>
    estimate_heavy_hitters(
        double frac_threshold,
        uint64_t tot_cnt) const;

private:
    void
    reset_delta();

    /*enum UpdateResult {
        MGUR_FOUND,
        MGUR_INSERTED,
        MGUR_SUBTRACTED,
        MGUR_SUBTRACTED_AND_INSERTED
    }; */

    //UpdateResult
    void
    update_impl(
        uint32_t                element,
        int                     cnt,
        std::list<uint32_t>     *p_deleted_list);

    double                  m_eps;

    uint32_t                m_k;

    mutable std::unordered_map<uint32_t, uint64_t>
                            m_cnt;

    mutable uint64_t        m_min_cnt;

    mutable uint64_t        m_delta;

    std::unordered_map<uint32_t, uint64_t>::size_type
                            m_initial_bucket_count;

public:
    static int
    unit_test(
        int argc,
        char *argv[]);

    friend class MisraGriesSketches::MisraGriesAccessor;
};

#endif // MISRA_GRIES_H


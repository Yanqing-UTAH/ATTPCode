#ifndef MISRA_GRIES_H
#define MISRA_GRIES_H

#include <unordered_map>

class MisraGries
{
public:
    MisraGries(
        double epsilon);

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

private:
    double            m_eps;

    uint32_t          m_k;

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
};

#endif // MISRA_GRIES_H


#ifndef SKETCH_H
#define SKETCH_H

// sketch.h is now decomposed into two files:
//  - The new sketch.h now includes all sketch interfaces and functions
//  - sketch_lib.h is a thin interface that driver depends on without including all
//    the implementation details.

#include <cmath>
#include <vector>
#include <cstdint>
#include <string>
#include "util.h"
#include "sketch_lib.h"

typedef unsigned long long TIMESTAMP;
using std::uint32_t;
using std::uint64_t;


/*
 * Note: ts > 0, following the notation in Wei et al. (Persistent Data Sketching)
 * Intervals are in the form of (ts_s, ts_e]
 */
struct IPersistentSketch
{
    virtual ~IPersistentSketch() {} ;
    
    virtual void
    clear() = 0;

    virtual size_t
    memory_usage() const = 0;
    
    // override IPersistentSketch::max_memory_usage() if the sketch's
    // memory usage fluctuates during updates.
    virtual size_t
    max_memory_usage() const { return memory_usage(); }

    virtual bool
    max_memory_usage_overriden() const { return false; }

    virtual std::string
    get_short_description() const = 0;

    static int num_configs_defined() { return -1; }
};

struct IPersistentSketch_str:
    virtual public IPersistentSketch
{
    // XXX the count c is not honored everywhere; fix the sketches
    // if it's needed in the test
    virtual void
    update(TIMESTAMP ts, const char *str, int c = 1) = 0;

};

struct IPersistentSketch_u32:
    virtual public IPersistentSketch
{
    // XXX the count c is not honored everywhere; fix the sketches
    // if it's needed in the test
    virtual void
    update(TIMESTAMP ts, uint32_t value, int c = 1) = 0;
};

struct IPersistentSketch_dvec:
    virtual public IPersistentSketch
{
    // ts: timestamp
    // dvec: n-dim 0-based array of doubles, where n is
    //       pre-specified in the constructor of the underlying
    //       sketch
    virtual void
    update(TIMESTAMP ts, const double *dvec) = 0;
};

struct IPersistentPointQueryable:
    virtual public IPersistentSketch_str
{
    virtual double 
    estimate_point_in_interval(
        const char *str,
        TIMESTAMP ts_s,
        TIMESTAMP ts_e) = 0;

    virtual double
    estimate_point_at_the_time(
        const char *str,
        TIMESTAMP ts_e) = 0;
};

struct AbstractPersistentPointQueryable:
    public IPersistentPointQueryable
{
    double
    estimate_point_in_interval(
        const char *str,
        TIMESTAMP ts_s,
        TIMESTAMP ts_e) override
    { return NAN; }

    double
    estimate_point_at_the_time(
        const char *str,
        TIMESTAMP ts_e) override
    { return NAN; }
};

struct HeavyHitter_u32
{
    uint32_t        m_value;     
    float           m_fraction;
};

struct IPersistentHeavyHitterSketch:
    virtual public IPersistentSketch_u32
{
    typedef HeavyHitter_u32 HeavyHitter;
    static constexpr const char *query_type = "heavy_hitter";

    virtual std::vector<HeavyHitter>
    estimate_heavy_hitters(
        TIMESTAMP ts_e,
        double frac_threshold) const = 0;
};

struct IPersistentHeavyHitterSketchBITP:
    virtual public IPersistentSketch_u32
{
    typedef HeavyHitter_u32 HeavyHitter;
    static constexpr const char *query_type = "heavy_hitter_bitp";

    virtual std::vector<HeavyHitter>
    estimate_heavy_hitters_bitp(
        TIMESTAMP ts_s,
        double frac_threshold) const = 0;
};

struct IPersistentFrequencyEstimationSketch:
    virtual public IPersistentSketch_u32
{
    static constexpr const char *query_type = "frequency_estimation";

    virtual uint64_t
    estimate_frequency(
        TIMESTAMP ts_e,
        uint32_t key) const = 0;
};

struct IPersistentFrequencyEstimationSketchBITP:
    virtual public IPersistentSketch_u32
{
    static constexpr const char *query_type = "frequency_estimation_bitp";

    virtual uint64_t
    estimate_frequency_bitp(
        TIMESTAMP ts_s,
        uint32_t key) const = 0;
};

struct IPersistentMatrixSketch:
    virtual public IPersistentSketch_dvec
{
    static constexpr const char *query_type = "matrix_sketch";

    // ts_e: end of the query period (inclusive)
    // A: The space where the covariance matrix is supposed to be stored.  Its
    // size should be at least n * (n + 1) / 2 if the sketch is configured to
    // accept vectors of size n.  The upper triangle will be stored in A in the
    // column major format.  For example, let M be the matrix, A[0] will be
    // M[0][0], A[1] will be M[0][1] and A[2] will be M[1][1] and so on.
    virtual void
    get_covariance_matrix(
        TIMESTAMP ts_e,
        double *A) const = 0;
};

IPersistentSketch*
create_persistent_sketch(
    SKETCH_TYPE st,
    int &argi,
    int argc,
    char *argv[],
    const char **help_str);

std::vector<IPersistentSketch*>
create_persistent_sketch_from_config(
    SKETCH_TYPE st);

#endif // SKETCH_H


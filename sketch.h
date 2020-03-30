#ifndef SKETCH_H
#define SKETCH_H

#include <cmath>
#include <vector>
#include <cstdint>
#include <string>
#include "util.h"

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

    virtual std::string
    get_short_description() const = 0;

    static int num_configs_defined() { return -1; }
};

struct IPersistentSketch_str:
    virtual public IPersistentSketch
{
    virtual void
    update(TIMESTAMP ts, const char *str, int c = 1) = 0;

};

struct IPersistentSketch_u32:
    virtual public IPersistentSketch
{
    virtual void
    update(TIMESTAMP ts, uint32_t value, int c = 1) = 0;
};

struct IPersistentSketch_dvec:
    virtual public IPersistentSketch
{
    // ts: timestamp
    // vec: n-dim 0-based array of doubles
    // n: dimension of the vector
    virtual void
    update(TIMESTAMP ts, double *vec, int n) = 0;
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

struct IPersistentMatrixSketch:
    virtual public IPersistentSketch_dvec
{
    // ts_e: end of the query period (inclusive)
    // exact_covariance_matrix: the ground truth
    // n: dimension of the covariance matrix (n * n)
    virtual double
    compute_exact_relative_error(
        TIMESTAMP ts_e,
        double *exact_covariance_matrix,
        int n) const = 0;
};

void setup_sketch_lib();

typedef int SKETCH_TYPE;
#define ST_INVALID -1
SKETCH_TYPE sketch_name_to_sketch_type(const char *sketch_name);
const char *sketch_type_to_sketch_name(SKETCH_TYPE st);
const char *sketch_type_to_altname(SKETCH_TYPE st);

IPersistentSketch*
create_persistent_sketch(
    SKETCH_TYPE st,
    int &argi,
    int argc,
    char *argv[],
    const char **help_str);

std::vector<SKETCH_TYPE>
check_query_type(
    const char *query_type,
    const char **help_str);

std::vector<IPersistentSketch*>
create_persistent_sketch_from_config(
    SKETCH_TYPE st);

#endif // SKETCH_H


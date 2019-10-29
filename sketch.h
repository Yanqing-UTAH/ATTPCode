#ifndef SKETCH_H
#define SKETCH_H

#include <cmath>

struct IPersistentSketch {
    virtual ~IPersistentSketch() {} ;

    virtual void 
    update(unsigned long long ts, const char *str, int c = 1) = 0;
    
    virtual void
    clear() = 0;

    virtual size_t
    memory_usage() = 0;
};

struct IPersistentPointQueryable: virtual public IPersistentSketch {
    virtual double 
    estimate_point_in_interval(
        const char *str,
        unsigned long long ts_s,
        unsigned long long ts_e) = 0;

    virtual double
    estimate_point_at_the_time(
        const char *str,
        unsigned long long ts_e) = 0;
};

struct AbstractPersistentPointQueryable: public IPersistentPointQueryable {
    double
    estimate_point_in_interval(
        const char *str,
        unsigned long long ts_s,
        unsigned long long ts_e) override
    { return NAN; }

    double
    estimate_point_at_the_time(
        const char *str,
        unsigned long long ts_e) override
    { return NAN; }
};


void setup_sketch_lib();

typedef int SKETCH_TYPE;
#define ST_INVALID -1
SKETCH_TYPE sketch_name_to_sketch_type(const char *sketch_name);

IPersistentPointQueryable*
create_persistent_point_queryable(
    SKETCH_TYPE st,
    int argc,
    char *argv[],
    const char **help_str);

#endif // SKETCH_H


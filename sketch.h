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

struct IPersistentPointQueryable {
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


#endif // SKETCH_H


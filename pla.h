#ifndef pla_h
#define pla_h

#include <limits>
#include <vector>
#include <cmath>
#include <iostream>
#include <cassert>


class PLA {
    public:
        struct point {
            unsigned long long x;
            double y;
        };

        struct segment{
            unsigned long long start, end;
            double slope;
            double intercept;
        };

        std::vector<segment> result;

        PLA(double);

        void clear();

        double estimate(unsigned long long) const;

        void feed(point current);

        unsigned long long memory_usage() const;

    private:

        const double double_eps = 0.000000001;

        double tollerance;

        double lower, upper;
        //available slope range : [lower, upper]
        point begin;
        //current time segment start point : begin
        point last;
        //last point
        
        bool initialized;

        double buffer_lower, buffer_upper;
        point buffer_begin, buffer_last;
        bool buffer_initialized;


        inline double valueAtTime(
            double slope, double intercept, unsigned long long time) const
        {
            return slope * time + intercept;
        }

        inline double pointSlope(point pt1, point pt2) const {
            assert((abs((double) pt2.x - pt1.x) > double_eps));
            return (pt2.y - pt1.y) / (pt2.x - pt1.x);
        }
};

#endif

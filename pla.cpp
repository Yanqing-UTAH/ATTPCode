#include "pla.h"

using namespace std;

PLA::PLA(double delta) : tollerance(delta) {
    clear();
}

void PLA::clear() {
    result.clear();
    buffer_lower = lower = numeric_limits<double>::lowest();
    buffer_upper = upper = numeric_limits<double>::max();
    buffer_initialized = initialized = false;
}

double PLA::estimate(unsigned long long t) const {
    if (buffer_initialized) {
        if (t >= buffer_last.x) {
            return buffer_last.y;
        } else if (t >= buffer_begin.x) {
            auto slope = (buffer_lower + buffer_upper) / 2;
            auto intercept = buffer_begin.y - slope * buffer_begin.x;
            return valueAtTime(slope, intercept, t);
        }
        int l = 0;
        int r = result.size() - 1;
        while (l <= r) {
            int m = (l + r) / 2;
            if (result[m].start <= t && result[m].end >= t) {
                return valueAtTime(result[m].slope, result[m].intercept, t);
            } else if (result[m].start > t) {
                r = m - 1;
            } else {
                l = m + 1;
            }
        }
    }
    return 0.0;
}

void PLA::feed(point current) {

    if (buffer_initialized && current.x - buffer_last.x > double_eps) {
        lower = buffer_lower;
        upper = buffer_upper;
        last = buffer_last;
        begin = buffer_begin;
        initialized = buffer_initialized;
    }
    else if (!result.empty() && abs((double) result.back().start - begin.x) < double_eps) {
        result.pop_back();
    }

    if (!initialized) {
        buffer_last = buffer_begin = current;
        buffer_initialized = true;
        return;
    }

    assert(current.x - last.x > - double_eps);

    auto lowerPrime = max(lower, pointSlope(begin, {current.x, current.y - tollerance}));
    auto upperPrime = min(upper, pointSlope(begin, {current.x, current.y + tollerance}));

    if (lowerPrime <= upperPrime) {
        buffer_lower = lowerPrime;
        buffer_upper = upperPrime;
    } else {
        //((upper+lower)/2)*(t-begin.x)+(begin.y)
        auto slope = (lower + upper) / 2;
        auto intercept = begin.y - slope * begin.x;
        result.push_back({begin.x, last.x, slope, intercept});

        buffer_begin = {last.x, valueAtTime(slope, intercept, last.x)};
        buffer_lower = pointSlope(buffer_begin, {current.x, current.y - tollerance});
        buffer_upper = pointSlope(buffer_begin, {current.x, current.y + tollerance});
    }
    buffer_last = current;
}

unsigned long long PLA::memory_usage() const {
    return result.capacity() * sizeof(segment) + sizeof(*this);
}

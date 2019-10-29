#ifndef util_h
#define util_h

#include <cmath>

#define STRINGIFY_HELPER(_1) #_1
#define STRINGIFY(_1) STRINGIFY_HELPER(_1)

#define CONCAT_HELPER(_1, _2) _1 ## _2
#define CONCAT(_1, _2) CONCAT_HELPER(_1, _2)

inline unsigned int hashstr(const char *str) {
	unsigned long hash = 5381;
	int c;
	while ((c = *str++)) {
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}
	return hash;
}

inline int rand_int() {
#define LONG_PRIME 32993
	return int(double(rand())*double(LONG_PRIME)/double(RAND_MAX) + 1);
}

inline bool check_double_ee(double value, double min, double max, char *str_end) {
    return !(!str_end || *str_end != '\0' ||
        value == HUGE_VAL || value <= min || value >= max);
}

#endif

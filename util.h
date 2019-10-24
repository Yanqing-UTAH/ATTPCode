#ifndef util_h
#define util_h

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

#endif

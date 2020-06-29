#ifndef util_h
#define util_h

#include <cmath>
#include <cerrno>
#include <cstring>
#include "MurmurHash3.h"

#define STRINGIFY_HELPER(_1) #_1
#define STRINGIFY(_1) STRINGIFY_HELPER(_1)

#define CONCAT_HELPER(_1, _2) _1 ## _2
#define CONCAT(_1, _2) CONCAT_HELPER(_1, _2)

#define CONCAT3_HELPER(_1, _2, _3) _1 ## _2 ## _3
#define CONCAT3(_1, _2, _3) CONCAT3_HELPER(_1, _2, _3)

// the ``garbage'' argument is required up until C++20 and for all C versions
#define SELECT_FIRST(...) SELECT_FIRST_HELPER(__VA_ARGS__, garbage)
#define SELECT_FIRST_HELPER(first, ...) first
#define SELECT_SECOND(...) SELECT_SECOND_HELPER(__VA_ARGS__, , garbage)
#define SELECT_SECOND_HELPER(first, second, ...) second

#define NOT(boolean_var) CONCAT(NOT_HELPER_, boolean_var)
#define NOT_HELPER_true false
#define NOT_HELPER_false true 

// IF_NONEMPTY(arg_to_test, if_branch[, else_branch])
#define IF_NONEMPTY(arg, ...) \
    CONCAT(SELECT_, CONCAT(IF_NONEMPTY_HELPER_, IS_EMPTY(arg))) (__VA_ARGS__)
#define IF_NONEMPTY_HELPER_true SECOND
#define IF_NONEMPTY_HELPER_false FIRST

// IF_EMPTY(arg_to_test, if_branch[, else_branch])
#define IF_EMPTY(arg, ...) \
    CONCAT(SELECT_, CONCAT(IF_NONEMPTY_HELPER_, NOT(IS_EMPTY(arg)))) (__VA_ARGS__)

// add an optional comma if condition holds
#define COMMA_true ,
#define COMMA_false 
#define IF_NONEMPTY_COMMA(arg, if_branch) \
    IF_NONEMPTY(arg, if_branch) CONCAT(COMMA_, NOT(IS_EMPTY(arg)))
#define IF_EMPTY_COMMA(arg, if_branch) \
    IF_EMPTY(arg, if_branch) CONCAT(COMMA_, IS_EMPTY(arg))

#define IF_BOOLEAN_LITERAL(arg, ...) \
    IF_EMPTY(CONCAT(IF_BOOLEAN_LITERAL_HELPER_, arg), __VA_ARGS__)
#define IF_BOOLEAN_LITERAL_HELPER_true
#define IF_BOOLEAN_LITERAL_HELPER_false

#define EXPAND_TO_COMMA(...) ,
#define HAS_COMMA_1_HELPER(_1, _2, _3, ...) _3
#define HAS_COMMA_1(...) HAS_COMMA_1_HELPER(__VA_ARGS__, t, f)
#define IS_EMPTY(arg) \
    CONCAT3(IS_EMPTY_HELPER_, \
    HAS_COMMA_1(EXPAND_TO_COMMA arg ()), \
    HAS_COMMA_1(EXPAND_TO_COMMA arg))
#define IS_EMPTY_HELPER_tf true
#define IS_EMPTY_HELPER_ff false
#define IS_EMPTY_HELPER_tt false

#define IS_NONEMPTY(arg) NOT(IS_EMPTY(arg))

#define HAS_ONLY_ONE___VA_ARGS__(...) \
    HAS_ONLY_ONE___VA_ARGS___HELPER(__VA_ARGS__,\
        f, f, f, f, f, f, f, f, f, f, \
        f, f, f, f, f, f, f, f, f, f, \
        f, f, f, f, f, f, f, f, f, f, \
        f, t, garbage)
#define HAS_ONLY_ONE___VA_ARGS___HELPER(\
   a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, \
   a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, \
   a21, a22, a23, a24, a25, a26, a27, a28, a29, a30, \
   a31, a32, a33, ...) a33

// Contrary to lisp: we can call CAR and CDR on empty __VA_ARGS__
// CAR() and CDR() produce empty
// Limit of 33 arguments
#define CAR SELECT_FIRST
#define CDR(...) \
    CONCAT(CDR_HELPER_, HAS_ONLY_ONE___VA_ARGS__(__VA_ARGS__))(__VA_ARGS__)
#define CDR_HELPER_t(...)
#define CDR_HELPER_f(first, ...) __VA_ARGS__
#define CADR(...) CAR(CDR(__VA_ARGS__))
#define CADDR(...) CAR(CDR(CDR(__VA_ARGS__)))
#define CADDDR(...) CAR(CDR(CDR(CDR(__VA_ARGS__))))
#define CADDDDR(...) CAR(CDR(CDR(CDR(CDR(__VA_ARGS__)))))
#define CDDDDDR(...) CDR(CDR(CDR(CDR(CDR(__VA_ARGS__)))))


const ssize_t help_str_bufsize = 65536ul;
extern char help_str_buffer[help_str_bufsize];

inline unsigned long hashstr_l(const char *str) {
	unsigned long hash = 5381;
	int c;
	while ((c = *str++)) {
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}
	return hash;
}

inline unsigned int hashstr(const char *str) {
    return (unsigned) hashstr_l(str);
}

inline int rand_int() {
#define LONG_PRIME 32993
	return int(double(rand())*double(LONG_PRIME)/double(RAND_MAX) + 1);
}

inline bool check_double_ee(double value, double min, double max, char *str_end) {
    return !(!str_end || *str_end != '\0' ||
        value == HUGE_VAL || value <= min || value >= max);
}

inline bool check_long_ii(long value, long min, long max, char *str_end) {
    return !(!str_end || *str_end != '\0' ||
       errno == ERANGE || value < min || value > max); 
}

template<class T>
struct ResourceGuard {
    ResourceGuard(T *t = nullptr) {
        m_t = t;
    }
    ResourceGuard(const ResourceGuard &) = delete;
    ResourceGuard& operator=(const ResourceGuard &) = delete;
    
    ResourceGuard(ResourceGuard &&other) noexcept: m_t(other.m_t) {
        other.m_t = nullptr; 
    }

    ResourceGuard& operator=(ResourceGuard &&other) noexcept {
        delete m_t;
        this.m_t = other.m_t;
        other.m_t = nullptr; 
    }

    ~ResourceGuard() {
        delete m_t;
    }

    T* operator->() const { return m_t; }
    T& operator*() const { return m_t; }

    T *get() const { return m_t; }

private:
    T *m_t;
};

inline uint64_t str_hash(const char *str)
{
    size_t len = strlen(str);
    uint64_t h[2];
    MurmurHash3_x64_128(str, len, 950810u, h);
    return h[0] ^ h[1];
}
    

#endif

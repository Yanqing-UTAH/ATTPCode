#ifndef BASIC_DEFS_H
#define BASIC_DEFS_H

#include <cstdint>
#include <cstddef>

namespace dsimpl {

typedef std::int16_t INT2;
typedef std::uint16_t UINT2;
typedef std::int32_t INT4;
typedef std::uint32_t UINT4;
typedef std::int64_t INT8;
typedef std::uint64_t UINT8;
typedef unsigned char UCHAR;
typedef UCHAR UINT1;
typedef UCHAR BYTE;
typedef double DOUBLE;

using std::size_t;
using std::uintptr_t;
using std::ptrdiff_t;

typedef ptrdiff_t PayloadOffset;

typedef UINT8 WEIGHT;
typedef INT8 WEIGHT_DIFF;

}

#endif // BASIC_DEF_H


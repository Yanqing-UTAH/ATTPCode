#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([ATTPCode], [0.1])

AC_ARG_ENABLE([debug], AS_HELP_STRING([--enable-debug], [enable debug build]))
AS_IF([test "$enable_debug" = "yes"], [
   CXXFLAGS="$CXXFLAGS -O0 -g -fkeep-inline-functions"
],[
   CPPFLAGS="$CPPFLAGS -DNDEBUG"
   CXXFLAGS="$CXXFLAGS -O2"
])


AC_CONFIG_SRCDIR([Makefile.in])
AH_TOP([
    #ifndef CONFIG_H
    #define CONFIG_H])
AC_CONFIG_HEADERS([config.h:config.h.in])
AH_BOTTOM([
    #endif // CONFIG_H])

# C compiler and c libraries
AC_PROG_CC
AC_LANG([C])

# Checks for header files.
AC_CHECK_HEADERS([lapacke.h lapacke_utils.h], [], [AC_MSG_ERROR([Missing liblapacke-dev])])
AC_CHECK_HEADER([cblas.h], [], [AC_MSG_ERROR([Missing CBLAS library])])
AC_CHECK_HEADER([fftw3.h], [], [AC_MSG_ERROR([Missing libfftw3-dev])])

# check cblas with the c compiler
AC_SEARCH_LIBS([cblas_ddot], [cblas], [], [AC_MSG_ERROR([CBLAS not working])])

LAPACK_dlange_name="`echo '
#include <lapacke.h>
LAPACK_dlange
' | gcc -E - | tail -n 1`"
AC_SEARCH_LIBS([$LAPACK_dlange_name], [lapack], [], [AC_MSG_ERROR([LAPACK not working])])


# C++ compiler and c++ libraries
AC_PROG_CXX
AC_LANG([C++])

# Checks for libraries.
AC_SEARCH_LIBS([LAPACKE_dgesv], [lapacke], [], [AC_MSG_ERROR([LAPACKE not working])])
AC_SEARCH_LIBS([fftw_plan_many_r2r], [fftw3], [], [AC_MSG_ERROR([fftw3 not working])])


# Checks for typedefs, structures, and compiler characteristics.
AX_CXX_COMPILE_STDCXX([17],[ext],[mandatory])

AC_TYPE_INT16_T
AC_TYPE_UINT16_T
AC_TYPE_INT32_T
AC_TYPE_UINT32_T
AC_TYPE_INT64_T
AC_TYPE_UINT64_T
AC_TYPE_UINTPTR_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_CHECK_TYPES(ptrdiff_t)

# Checks for library functions.

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([stamp-h], [echo timestamp > stamp-h])
AC_OUTPUT

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
AC_CHECK_HEADERS([lapacke.h])
AC_CHECK_HEADERS([lapacke_utils.h])
AC_CHECK_HEADERS([cblas.h])

# check cblas with the c compiler
AC_SEARCH_LIBS([cblas_ddot], [cblas])

LAPACK_dlange_name="`echo '
#include <lapacke.h>
LAPACK_dlange
' | gcc -E - | tail -n 1`"
AC_SEARCH_LIBS([$LAPACK_dlange_name], [lapack])


# C++ compiler and c++ libraries
AC_PROG_CXX
AC_LANG([C++])

# Checks for libraries.
AC_SEARCH_LIBS([LAPACKE_dgesv], [lapacke])


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

#AC_CHECK_TYPES(__int128)
#AC_CHECK_TYPES(unsigned __int128)

#AC_CHECK_DECLS([strdup])

#AC_COMPILE_IFELSE(
#[AC_LANG_SOURCE([[
##define MSG(msg, ...) fprintf(stderr, msg, ##__VA_ARGS__)
#int main() { MSG("msg"); return 0; }
#]])],
#AC_DEFINE([SUPPORT_MACRO_VA_ARGS_COMMA_REMOVAL],[1],[
#        Define to 1 if the preprocessor supports the syntax of using ##__VA_ARGS__
#        to remove the trailing comma prior to __VA_ARGS__
#    ])
#)

# Checks for library functions.

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([stamp-h], [echo timestamp > stamp-h])
AC_OUTPUT

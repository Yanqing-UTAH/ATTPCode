/* missing function APIs in LAPACKE */
/* An ANSI C header */
#ifndef LAPACK_WRAPPER_H
#define LAPACK_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

double lapack_wrapper_dlansp(
    char            norm,
    char            uplo,
    lapack_int      n,
    const double    *ap);

#ifdef __cplusplus
}
#endif

#endif /* LAPACK_WRAPPER_H */


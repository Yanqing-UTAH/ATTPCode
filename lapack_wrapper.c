#include <lapacke.h>
#include <lapacke_utils.h>

#define LAPACK_dlansp LAPACK_GLOBAL(dlansp,DLANSP)
double LAPACK_dlansp(
    char const* norm, char const* uplo,
    lapack_int const* n,
    double const* AP,
    double* work);

double lapack_wrapper_dlansp(
    char            norm,
    char            uplo,
    lapack_int      n,
    const double    *ap)
{
    lapack_int info = 0;
    double res = 0.;
    double *work = NULL;

    /* we don't check for NAN here */
    
    if (LAPACKE_lsame(norm, 'i') ||
        LAPACKE_lsame(norm, '1') ||
        LAPACKE_lsame(norm, 'o'))
    {
        work = (double*) LAPACKE_malloc(sizeof(double) * MAX(1, n));
        if (!work)
        {
            info = LAPACK_WORK_MEMORY_ERROR;
            goto lapack_wrap_dlansp_error;
        }
    }
    
    res = LAPACK_dlansp(
        &norm,
        &uplo,
        &n,
        ap,
        work);
        
    if (LAPACKE_lsame(norm, 'i') ||
        LAPACKE_lsame(norm, '1') ||
        LAPACKE_lsame(norm, 'o'))
    {
        LAPACKE_free(work);
    }

lapack_wrap_dlansp_error:
    if (info == LAPACK_WORK_MEMORY_ERROR)
    {
        LAPACKE_xerbla("lapack_wrap_dlansp", info);
    }
    return res;
}

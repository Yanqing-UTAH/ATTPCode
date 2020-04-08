#include <iostream>
#include <lapacke.h>
using namespace std;

int main(int argc, char *argv[]) {
    
    constexpr lapack_int m = 2, n = 3;
    double A[n][m] = {
        {1, -2},
        {0, 1},
        {-1, 4}
    };
    double S[m];
    double U[m][m];
    double VT[n][n];
    
    lapack_int info = LAPACKE_dgesdd(
        LAPACK_COL_MAJOR,
        'A',
        m,
        n,
        *A,
        m, // LDA
        S,
        *U,
        m, // LDU
        *VT,
        n); // LDVT
    
    if (info != 0) {
        std::cout << "Something went wrong with dgesdd, INFO = "  << info << std::endl;
        return 1;
    }
    
    std::cout << "U = [" << std::endl;
    for (int i = 0; i != m; ++i) {
        for (int j = 0; j != m; ++j) {
            std::cout << '\t' << U[j][i];
        }
        std::cout << std::endl;
    }
    std::cout << ']' << std::endl;
    
    std::cout << std::endl << "S = diag(" << std::endl;
    for (int i = 0; i < m; ++i) {
        std::cout << S[i] << std::endl;
    }
    std::cout << ')' << std::endl;

    std::cout << std::endl << "V = " << std::endl;
    for (int i = 0; i != n; ++i) {
        for (int j = 0; j != n; ++j) {
            std::cout << '\t' << VT[i][j];
        }
        std::cout << std::endl;
    }
    std::cout << ']' << std::endl;

    return 0;
}

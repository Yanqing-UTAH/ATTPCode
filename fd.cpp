#include "fd.h"
#include "conf.h"
#include <iostream>
#include <algorithm>
#include <vector>
extern "C"
{
#include <lapacke.h>
#include <lapacke_utils.h>
#include <cblas.h>
}
#include <cassert>
#include <cstring>

using namespace std;

// fast-FD impl.
class FD {
    uint32_t d;
    uint32_t l;
    uint32_t first_zero_line;

    double *B; //a 2l*d matrix
    

public:
    FD(uint32_t _l, uint32_t _d) :
        d(_d),
        l(_l),
        first_zero_line(0),
        B(new double[2 * _l * _d]) {

        memset(&B[0], 0, sizeof(double) * 2 * l * d);
    }

    FD(const FD& other):
        d(other.d),
        l(other.l),
        first_zero_line(other.first_zero_line),
        B(new double[2 * other.l * other.d]) {
        
        memcpy(B, other.B, sizeof(double) * 2 * l * d);
    }

    ~FD() {
        delete []B;
    }

    void clear() {
        memset(&B[0], 0, sizeof(double) * 2 * l * d);
        first_zero_line = 0;
    }

    void update(const double *row) {
        if (first_zero_line == 2 * l) {
            double *S = new double[std::min(2 * l, d)];
            double *U = new double[2 * l * 2 * l];
            double *VT = new double[d * d];

#       ifdef NDEBUG
            (void)
#       else
            lapack_int info =
#       endif
            LAPACKE_dgesdd(
                LAPACK_COL_MAJOR,
                'A',
                2 * l,
                d,
                &B[0],
                2 * l, // LDA
                S,
                U,
                2 * l, // LDU
                VT,
                d); // LDVT
            /*if (info != 0) {
                LAPACKE_xerbla("dgesdd", info);        
                std::cout << 2 * l << ' ' << d << std::endl;
                std::cout << LAPACKE_dge_nancheck(
                    LAPACK_COL_MAJOR,
                    2 * l,
                    d,
                    &B[0],
                    2 * l) << std::endl;
            } */
            assert(!info);
    
            memset(B, 0, sizeof(double) * 2 * l * d);
            double epsilon = S[l - 1] * S[l - 1];
            for (first_zero_line = 0; first_zero_line < l - 1; ++first_zero_line) {
                S[first_zero_line] =
                    std::sqrt(S[first_zero_line] * S[first_zero_line] - epsilon);
                if (S[first_zero_line] == 0) {
                    break;
                }
            }
            
            for (uint32_t j = 0; j < d; ++j) {
                auto idx = j * 2 * l;
                auto idx2 = j * d;
                for (uint32_t i = 0; i < first_zero_line; ++i) {
                    B[idx++] = S[i] * VT[idx2++];
                }
            }
        
            delete []S;
            delete []U;
            delete []VT;
        }

        assert(first_zero_line < 2 * l); 
        //memcpy(&B[(first_zero_line++) * d], row, sizeof(double) * d);
        uint32_t idx = first_zero_line++;
        for (uint32_t i = 0; i < d; ++i) {
            B[idx] = row[i];
            idx += 2 * l;
        }
    }
    
    void pop_first(double *first_row) {
        assert(first_zero_line);
        //memcpy(first_row, &B[0], sizeof(double) * d);
        //memmove(&B[0], &B[0] + d, sizeof(double) * (first_zero_line - 1) * d);
        //memset(&B[(first_zero_line - 1) * d], 0, sizeof(double) * d);
        uint32_t idx = 0;
        for (uint32_t i = 0; i < d; ++i) {
            first_row[i] = B[idx];
            memmove(&B[idx], &B[idx + 1], sizeof(double) * (first_zero_line - 1));
            B[idx + first_zero_line - 1] = 0;
            idx += 2 * l;
        }
        --first_zero_line;
    }

    void to_matrix(double *B_out) {
        /*LAPACKE_dge_trans(
            LAPACK_ROW_MAJOR,
            2 * l,
            d,
            B,
            d,
            B_out,
            2 * l); */

        memcpy(B_out, B, sizeof(double) * 2 * l * d);
    }

    size_t memory_usage() const {
        return sizeof(FD) + sizeof(double) * 2 * l * d;
    }

    void
    to_covariance_matrix(double *A)
    {
        // A is a col-major packed upper triangle matrix
        memset(A, 0, sizeof(double) * d * (d + 1) / 2);
        size_t i = 0, sz = first_zero_line;
        for (; i < sz; ++i)
        {
            cblas_dspr(
                CblasColMajor,
                CblasUpper,
                d,
                1.0, // alpha ?? this one should be something else?
                B + i,
                2 * l,
                A);
        }
    }
};

// FD_ATTP implementation

FD_ATTP::FD_ATTP(int _l, int _d):
    l(_l),
    d(_d),
    AF2(0),
    nxt_target(0),
    C(new FD(_l, _d)),
    partial_ckpt(),
    full_ckpt()
{
}

FD_ATTP::~FD_ATTP()
{
    clear();
    delete C;
}

void
FD_ATTP::clear()
{
    AF2 = 0.0;
    C->clear();
    for (auto &pckpt: partial_ckpt) {
        delete []pckpt.row;
    }
    partial_ckpt.clear();
    for (auto &fckpt: full_ckpt) {
        delete fckpt.fd;
    }
    full_ckpt.clear();
}

size_t
FD_ATTP::memory_usage() const
{
    //std::cout << full_ckpt.size() <<  ' ' << partial_ckpt.size() << std::endl;
    return sizeof(FD_ATTP) + C->memory_usage() +
        (partial_ckpt.size() * (sizeof(PartialCkpt) + d * sizeof(double))) +
        ((full_ckpt.empty()) ? 0 :
         (full_ckpt.size() * (sizeof(FullCkpt) + full_ckpt.front().fd->memory_usage())));
}

std::string
FD_ATTP::get_short_description() const
{
    return std::string("PFD-l") + std::to_string(l);
}

void
FD_ATTP::update(
    TIMESTAMP ts,
    const double *a)
{
    //static unsigned long _cnt = 0;
    //++_cnt;
    C->update(a);
    
    double n2 = cblas_ddot(d, a, 1, a, 1);
    AF2 += n2;

    //std::cout << AF2 << std::endl;

    if (AF2 * (l-1) / l < nxt_target) {
        //std::cout << AF2 * (l - 1) / l << ' ' << nxt_target << std::endl;
        return;
    }
    
    double *CM = new double[2 * l * d];
    double *S = new double[std::min(2 * l, d)];
    
    double c1_2norm_sqr;
    for (;;) {
        C->to_matrix(CM);
#ifdef NDEBUG
        (void)
#else
        lapack_int info =
#endif
        LAPACKE_dgesdd(
            LAPACK_COL_MAJOR,
            'N',
            2*l,
            d,
            &CM[0],
            2 * l, // LDA
            &S[0],
            nullptr,
            2 * l, // LDU
            nullptr,
            d); // LDVT

        assert(!info);

        c1_2norm_sqr = S[0] * S[0];
        if (c1_2norm_sqr >= AF2/l) {
            double *row = new double[d];
            C->pop_first(row);

            uint32_t ckpt_cnt = full_ckpt.empty() ?
                partial_ckpt.size() :
                (partial_ckpt.size() - full_ckpt.back().next_partial_ckpt);
            if (ckpt_cnt + 1 >= l) {
                if (full_ckpt.empty()) {
                    full_ckpt.push_back(FullCkpt{
                        ts,
                        new FD(l, d),
                        (uint32_t) partial_ckpt.size()});
                } else {
                    full_ckpt.push_back(FullCkpt{
                        ts,
                        new FD(*full_ckpt.back().fd),
                        (uint32_t) partial_ckpt.size()});
                }
                auto new_fd = full_ckpt.back().fd;
                std::for_each(partial_ckpt.end() - ckpt_cnt, partial_ckpt.end(),
                    [new_fd](const PartialCkpt &p) {
                    new_fd->update(p.row);
                });
                new_fd->update(row);

                delete []row;
            } else {
                partial_ckpt.push_back(PartialCkpt{ts, row});
            }
        } else {
            break;
        }
    }

    nxt_target = AF2 - c1_2norm_sqr;
    
    delete []CM;
    delete []S;
}


void
FD_ATTP::get_covariance_matrix(
    TIMESTAMP ts_e,
    double *a) const
{
    auto full_cmp = [](TIMESTAMP ts_e, const FullCkpt &fckpt) -> bool {
        return ts_e < fckpt.ts;
    };
    auto iter = std::upper_bound(full_ckpt.begin(), full_ckpt.end(), ts_e, full_cmp);
    FD *fd;
    uint32_t pckpt_i;
    if (iter == full_ckpt.begin()) {
        fd = new FD(l, d);
        pckpt_i = 0;
    }
    else {
        fd = new FD(*((iter-1)->fd));
        pckpt_i = (iter-1)->next_partial_ckpt;
    }

    for (; pckpt_i < partial_ckpt.size() && partial_ckpt[pckpt_i].ts <= ts_e; ++pckpt_i) {
        fd->update(partial_ckpt[pckpt_i].row);
    }
    fd->to_covariance_matrix(a);
    delete fd;
}

FD_ATTP*
FD_ATTP::get_test_instance()
{
    return new FD_ATTP(1, 1);
}

FD_ATTP*
FD_ATTP::create_from_config(
    int idx)
{
    int n; // i.e., d
    uint32_t l;

    n = (int) g_config->get_u32("MS.dimension").value();
    l = g_config->get_u32("PFD.half_sketch_size", idx).value();

    return new FD_ATTP(l, n);
}

int
FD_ATTP::num_configs_defined()
{
    if (g_config->is_list("PFD.half_sketch_size"))
    {
        return (int) g_config->list_length("PFD.half_sketch_size");
    }

    return -1;
}


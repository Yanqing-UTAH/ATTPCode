#include "fd.h"
#include "conf.h"
#include <iostream>
#include <algorithm>
#include <vector>
extern "C"
{
#include <lapacke.h>
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

    std::vector<double> B; //a 2l*d matrix
    

public:
    FD(uint32_t _l, uint32_t _d) :
        d(_d),
        l(_l),
        first_zero_line(0),
        B(2*l*d, 0) {
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
                LAPACK_ROW_MAJOR,
                'A',
                2 * l,
                d,
                &B[0],
                d, // LDA,
                S,
                U,
                2 * l,
                VT,
                d);
            assert(!info);
    
            double epsilon = S[l] * S[l];
            uint32_t i;
            for (i = 0; i < l - 1; ++i) {
                auto s = std::sqrt(S[i] * S[i] - epsilon);
                if (s == 0)
                    break;
                auto idx = i * d;
                for (uint32_t j = 0; j < d; ++j) {
                    B[idx] = s * VT[idx];
                    ++idx;
                }
            }
            memset(&B[i * d], 0, sizeof(double) * (2 * l - i) * d);
            first_zero_line = i;
        
            delete []S;
            delete []U;
            delete []VT;
        }
        
        memcpy(&B[(first_zero_line++) * d], row, sizeof(double) * d);
    }
    
    void pop_first(double *first_row) {
        assert(first_zero_line);
        memcpy(first_row, &B[0], sizeof(double) * d);
        memmove(&B[0], &B[0] + d, sizeof(double) * (first_zero_line - 1) * d);
        memset(&B[(first_zero_line - 1) * d], 0, sizeof(double) * d);
        --first_zero_line;
    }

    vector<double> to_matrix() {
        return B;
    }

    size_t memory_usage() const {
        // TODO ??
        return 2 * l * d * sizeof(double);
    }

    void
    to_covariance_matrix(double *A)
    {
        // A is a col-major packed upper triangle matrix
        memset(A, 0, sizeof(double) * d * (d + 1) / 2);
        size_t i = 0, sz = first_zero_line * d; 
        for (; i < sz; i += d)
        {
            cblas_dspr(
                CblasColMajor,
                CblasUpper,
                d,
                1.0, // alpha ?? this one should be something else?
                B.data() + i,
                1,
                A);
        }
    }
};

// FD_ATTP implementation

FD_ATTP::FD_ATTP(int _l, int _d):
    l(_l),
    d(_d),
    C(new FD(l, d)),
    AF2(0),
    partial_ckpt(),
    full_ckpt(),
    ckpt_cnt(0) {
    }

FD_ATTP::~FD_ATTP()
{
    // TODO
}

void
FD_ATTP::clear()
{
    // TODO
}

size_t
FD_ATTP::memory_usage() const
{
    // TODO
    return 0;
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
    static unsigned long _cnt = 0;
    ++_cnt;
    C->update(a);

    AF2 += cblas_ddot(d, a, 1, a, 1);
    
    auto CM = C->to_matrix();
    std::vector<double> S(d);
#ifdef NDEBUG
    (void)
#else
    lapack_int info =
#endif
    LAPACKE_dgesdd(
        LAPACK_ROW_MAJOR,
        'N',
        2*l,
        d,
        &CM[0],
        d, // LDA
        &S[0],
        nullptr,
        2 * l, // LDU
        nullptr,
        d); // LDVT
    assert(!info);
    double c1_2norm_sqr = S.front() * S.front();
    if (c1_2norm_sqr >= AF2/l) {
        double *row = new double[d];
        C->pop_first(row);
        //partial_ckpt.push_back(PartialCkpt{ts, new double[d]});
        //C->pop_first(&partial_ckpt.back().row[0]);
        //++ckpt_cnt;
        std::cout << "partial: " << _cnt << std::endl;

        if (ckpt_cnt + 1 >= l) {

            std::cout << "full: " << _cnt << std::endl;
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
            ckpt_cnt = 0;

            delete []row;
        } else {
            partial_ckpt.push_back(PartialCkpt{ts, row});
            ++ckpt_cnt;
        }
    }
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


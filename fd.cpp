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
    vector<double> B; //a 2l*d matrix
    size_t p_all_zeros;
    vector<double> U;
    vector<double> S;
    vector<double> VT;

    public:
    int d;
    int l;

    FD(int _l, int _d) :
        B(vector<double>(2*l*d, 0)),
        p_all_zeros(0),
        U(vector<double> (2*l*2*l)),
        S(vector<double> (d)),
        VT(vector<double> (d*d)),
        d(_d),
        l(_l)
        {}

    void update(double *row) {
        //assert(row.size() == d);
        p_all_zeros = copy(row, row + d, B.begin() + p_all_zeros) - B.begin();
        if (p_all_zeros == B.size()) {
#       ifdef NDEBUG
            (void)
#       else
            lapack_int info = 
#       endif
            LAPACKE_dgesdd(
                LAPACK_ROW_MAJOR,
                'A',
                2*l,
                d,
                &B[0],
                d, // LDA
                &S[0],
                &U[0],
                2*l, // LDU
                &VT[0],
                d); // LDVT
            assert(!info);
            for (int i = 0; i < l-1; ++i) {
                for (int j = 0; j < d; ++j) {
                    B[i*2*l+j] = VT[i*d+j] * sqrt(S[i] * S[i] - S[l]);
                }
            }
            p_all_zeros = (l-1) * d;
        }
    }

    vector<double> to_matrix() {
        fill(B.begin() + p_all_zeros, B.end(), 0);
        return B;
    }

    size_t memory_usage() const {
        return 2 * l * d * sizeof(double);
    }

    void
    to_covariance_matrix(double *A)
    {
        // A is a col-major packed upper triangle matrix
        memset(A, 0, sizeof(double) * d * (d + 1) / 2);
        size_t i = 0, sz = p_all_zeros; 
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
    partial_ckpt(vector<pair<TIMESTAMP, vector<double>>>()),
    full_ckpt({{0, new FD(l, d)}}),
    ckpt_cnt(0) {
    }

FD_ATTP::~FD_ATTP()
{
    delete C;
    for (auto &p: full_ckpt)
    {
        delete p.second;
    }
}

void
FD_ATTP::clear()
{
    // TODO
}

size_t
FD_ATTP::memory_usage() const
{
    size_t full_ckpt_usage = full_ckpt.size() == 0 ? 0 : full_ckpt.size() * (full_ckpt.front().second->memory_usage() + sizeof(int));
    size_t partial_ckpt_usage = partial_ckpt.size() == 0 ? 0 : partial_ckpt.size() * (partial_ckpt.front().second.size() * sizeof(double) + sizeof(int));
    return full_ckpt_usage + partial_ckpt_usage + C->memory_usage();
}

std::string
FD_ATTP::get_short_description() const
{
    return std::string("PFD-l") + std::to_string(l);
}

void
FD_ATTP::update(
    TIMESTAMP ts,
    double *a)
{
    C->update(a);

    AF2 += cblas_ddot(d, a, 1, a, 1);
    //for (auto& i : a) {
    //    AF2 += i * i;
    //}
    
    auto CM = C->to_matrix();
    vector<double> S(d);
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
        2*l, // LDU
        nullptr,
        d); // LDVT
    assert(!info);
    if (S.front() >= AF2/l) {
        partial_ckpt.push_back({ts, std::vector(a, a + d)});
        if (ckpt_cnt >= l) {
            full_ckpt.emplace_back(std::make_pair(full_ckpt.back().first, new FD(*full_ckpt.back().second)));
            for_each(partial_ckpt.end() - ckpt_cnt, partial_ckpt.end(), [&](decltype(partial_ckpt)::value_type p) {
                full_ckpt.back().second->update(p.second.data());
            });
            for (auto i = partial_ckpt.end() - ckpt_cnt; i != partial_ckpt.end(); ++i) {
            }
            ckpt_cnt = 0;
        }
    }
}


void
FD_ATTP::get_covariance_matrix(
    TIMESTAMP ts,
    double *a) const
{
    //a.resize(l * d);
    auto full_cmp = [](TIMESTAMP ts_e, const decltype(full_ckpt)::value_type &p) -> bool {
        return ts_e < p.first;
    };
    auto iter = upper_bound(full_ckpt.begin(), full_ckpt.end(), ts, full_cmp);
    if (iter == full_ckpt.begin()) {
        memset(a, 0, sizeof(double) * d * (d + 1) / 2);
    }
    else {
        FD fd(*((iter - 1)->second));
        auto partial_cmp = [](TIMESTAMP ts_e, const decltype(partial_ckpt)::value_type &p) -> bool {
            return ts_e < p.first;
        };
        auto s = upper_bound(partial_ckpt.begin(), partial_ckpt.end(), iter->first, partial_cmp);
        auto t = upper_bound(partial_ckpt.begin(), partial_ckpt.end(), ts, partial_cmp);
        for_each(s, t, [&fd](decltype(partial_ckpt)::value_type p) {
            fd.update(p.second.data());
        });

        fd.to_covariance_matrix(a);
    }
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


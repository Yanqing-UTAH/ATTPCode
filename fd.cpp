#include <iostream>
#include <algorithm>
#include <vector>
#include <lapacke.h>
#include <cassert>
#include "sketch.h"

using namespace std;

class FD {
    vector<double> B; //a 2l*d matrix
    int p_all_zeros;
    vector<double> U;
    vector<double> S;
    vector<double> VT;

    public:
    int d;
    int l;

    FD(int _l, int _d) :
        l(_l),
        d(_d),
        B(vector<double>(2*l*d, 0)),
        p_all_zeros(0),
        U(vector<double> (2*l*2*l)),
        S(vector<double> (d)),
        VT(vector<double> (d*d))
        {}

    void update(vector<double> row) {
        assert(row.size() == d);
        p_all_zeros = copy(row.begin(), row.end(), B.begin() + p_all_zeros) - B.begin();
        if (p_all_zeros == B.size()) {
            lapack_int info = LAPACKE_dgesdd(
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
};

class FD_ATTP {
    int l;
    int d;
    FD C;
    double AF2;
    vector<pair<TIMESTAMP, vector<double>>> partial_ckpt;
    vector<pair<TIMESTAMP, FD>> full_ckpt;
    int ckpt_cnt;

    public:

    FD_ATTP(int _l, int _d) :
    l(_l),
    d(_d),
    C(FD(l, d)),
    AF2(0),
    partial_ckpt(vector<pair<TIMESTAMP, vector<double>>>()),
    full_ckpt({{0, FD(l, d)}}),
    ckpt_cnt(0) {
    }

    void update(TIMESTAMP ts, vector<double> a) {
        C.update(a);
        for (auto& i : a) {
            AF2 += i * i;
        }
        auto CM = C.to_matrix();
        vector<double> S(d);
        lapack_int info = LAPACKE_dgesdd(
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
            partial_ckpt.push_back({ts, a});
            if (ckpt_cnt >= l) {
                full_ckpt.push_back(full_ckpt.back());
                for_each(partial_ckpt.end() - ckpt_cnt, partial_ckpt.end(), [&](decltype(partial_ckpt)::value_type p) {
                    full_ckpt.back().second.update(p.second);
                });
                for (auto i = partial_ckpt.end() - ckpt_cnt; i != partial_ckpt.end(); ++i) {
                }
                ckpt_cnt = 0;
            }
        }
    }

    void query(TIMESTAMP ts, vector<double> &a) {
        a.resize(l * d);
        auto full_cmp = [](TIMESTAMP ts_e, const decltype(full_ckpt)::value_type &p) -> bool {
            return ts_e < p.first;
        };
        auto iter = upper_bound(full_ckpt.begin(), full_ckpt.end(), ts, full_cmp);
        if (iter == full_ckpt.begin()) {
            fill(a.begin(), a.end(), 0);
        }
        else {
            FD fd = (iter - 1)->second;
            auto partial_cmp = [](TIMESTAMP ts_e, const decltype(partial_ckpt)::value_type &p) -> bool {
                return ts_e < p.first;
            };
            auto s = upper_bound(partial_ckpt.begin(), partial_ckpt.end(), iter->first, partial_cmp);
            auto t = upper_bound(partial_ckpt.begin(), partial_ckpt.end(), ts, partial_cmp);
            for_each(s, t, [&fd](decltype(partial_ckpt)::value_type p) {
                fd.update(p.second);
            });
            a = fd.to_matrix();
        }
    }

    size_t memory_usage() const {
        size_t full_ckpt_usage = full_ckpt.size() == 0 ? 0 : full_ckpt.size() * (full_ckpt.front().second.memory_usage() + sizeof(int));
        size_t partial_ckpt_usage = partial_ckpt.size() == 0 ? 0 : partial_ckpt.size() * (partial_ckpt.front().second.size() * sizeof(double) + sizeof(int));
        return full_ckpt_usage + partial_ckpt_usage + C.memory_usage();
    }

};
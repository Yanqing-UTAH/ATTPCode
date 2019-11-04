#include "pams.h"

using namespace std;

PAMSketch::PAMSketch(double eps, double delta, double Delta) :
    w(ceil(exp(1)/eps)),
    d(ceil(log(1/delta))),
    D(Delta),
    p_sampling(1/Delta) {

    C.resize(d);
    for (unsigned int i = 0; i < d; i++) {
        C[i].resize(w);
    }

    //srand(time(NULL));
    hashes.resize(d);
    ksi.resize(d);
    for (unsigned int i = 0; i < d; i++) {
        hashes[i].first = rand_int();
        hashes[i].second = rand_int();
        ksi[i] = std::make_tuple(rand_int(), rand_int(), rand_int(), rand_int());
    }
}

void PAMSketch::clear() {
    for (unsigned i = 0; i < d; ++i) {
        C[i].clear();
        C[i].resize(w);
    }
}

void PAMSketch::update(unsigned long long t, const char *str, int c) {
    unsigned int item = hashstr(str);
    for (unsigned int j = 0; j < d; j++) {
        unsigned int hashval = h(j, item);
        unsigned int f = flag(j, item);
        C[j][hashval][f].val += c;

        if (p_sampling(rgen)) {
            C[j][hashval][f].samples.emplace_back(std::make_pair(t, C[j][hashval][f].val));
        }
    }
}

double
PAMSketch::estimate_point_in_interval(
    const char *str,
    unsigned long long s,
    unsigned long long e) {
    unsigned int item = hashstr(str);
    
    std::vector<double> D;
    D.reserve(d);
    for (unsigned int j = 0; j < d; j++) {
        unsigned int hashval = h(j, item);
        int Xi_value = Xi(j, hashval);
        double D_s = Xi_value * (estimate_C(j, hashval, 1, s) - estimate_C(j, hashval, 0, s));
        double D_e = Xi_value * (estimate_C(j, hashval, 1, e) - estimate_C(j, hashval, 0, e));
        
        D.push_back(D_e - D_s);
    }

    std::sort(D.begin(), D.end());
    if (d & 1) {
        return D[d/2];
    } else {
        return (D[d/2] + D[d/2 - 1]) / 2.;
    }
}

double
PAMSketch::estimate_point_at_the_time(
    const char *str,
    unsigned long long ts_e) {

    return estimate_point_in_interval(str, 0, ts_e);
}

double PAMSketch::estimate_C(unsigned j, unsigned i, unsigned f, unsigned long long t) {
    auto &samples = C[j][i][f].samples;

    auto r = samples.size(), l = (decltype(r)) 0;
    
    while (l < r) {
        auto mid = (l + r) / 2;
        if (samples[mid].first == t) {
            l = mid + 1;
            break;
        } else if (samples[mid].first < t) {
            l = mid + 1;
        } else {
            r = mid;
        }
    }
    
    if (l == 0) {
        return 0;
    } else {
        return samples[l - 1].second + D - 1;
    }
}

size_t PAMSketch::memory_usage() {
    size_t mem = 0;
    
    mem += 2 * sizeof(Counter) * w * d;
    for (unsigned j = 0; j < d; ++j)
    for (unsigned h = 0; h < w; ++h) {
        mem += sizeof(C[j][h][0].samples.front()) *
            (C[j][h][0].samples.size() + C[j][h][1].samples.size());
    }

    return mem;
}

PAMSketch *PAMSketch::create(int argc, char *argv[], const char **help_str) {
    if (argc < 3) {
        if (help_str) *help_str = " <epsilon> <delta> <Delta>";
        return nullptr;
    }
    
    char *str_end;
    double epsilon = std::strtod(argv[0], &str_end);
    if (!check_double_ee(epsilon, 0, 1, str_end)) {
        if (help_str) *help_str = " <epsilon> <detal> <Delta>\n[Error] Invalid epsilon\n";
        return nullptr;
    }
    double delta = std::strtod(argv[0], &str_end);
    if (!check_double_ee(delta, 0, 1, str_end)) {
        if (help_str) *help_str = " <epsilon> <detal> <Delta>\n[Error] Invalid delta\n";
        return nullptr;
    }
    double Delta = std::strtod(argv[0], &str_end);
    if (!check_double_ee(Delta, 0, INFINITY, str_end)) {
        if (help_str) *help_str = " <epsilon> <detal> <Delta>\n[Error] Invalid Delta\n";
        return nullptr;
    }

    return new PAMSketch(epsilon, delta, Delta);
}


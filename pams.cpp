#include "pams.h"
#include "conf.h"
#include <sstream>

using namespace std;

PAMSketch::PAMSketch(double eps, double delta, double Delta) :
    w(ceil(exp(1)/eps)),
    d(ceil(log(1/delta))),
    D(Delta),
    p_sampling(1/Delta),
    m_eps(eps),
    m_delta(delta),
    m_Delta(Delta) {

    C.resize(d);
    for (unsigned int i = 0; i < d; i++) {
        C[i].resize(w);
    }

    //srand(time(NULL));
    ksi.resize(d);
    for (unsigned int i = 0; i < d; i++) {
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
	size_t len = strlen(str);
    unsigned int item = hashstr(str);
    for (unsigned int j = 0; j < d; j++) {
        unsigned int hashval = h(j, str, len);
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
    size_t len = strlen(str);
    
    std::vector<double> D;
    D.reserve(d);
    for (unsigned int j = 0; j < d; j++) {
        unsigned int hashval = h(j, str, len);
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

size_t PAMSketch::memory_usage() const {
    size_t mem = 0;
    
    mem += 2 * sizeof(Counter) * w * d;
    for (unsigned j = 0; j < d; ++j)
    for (unsigned h = 0; h < w; ++h) {
        mem += sizeof(C[j][h][0].samples.front()) *
            (C[j][h][0].samples.size() + C[j][h][1].samples.size());
    }

    return mem;
}

std::string PAMSketch::get_short_description() const {
    std::ostringstream oss;
    oss << std::fixed << "PAMS-e" << m_eps << "-d" << m_delta << "-D" << m_Delta;
    return oss.str();
}

PAMSketch *PAMSketch::create(int &argi, int argc, char *argv[], const char **help_str) {
    if (argi + 3 > argc) {
        if (help_str) *help_str = " <epsilon> <delta> <Delta>\n";
        return nullptr;
    }
    
    char *str_end;
    double epsilon = std::strtod(argv[argi++], &str_end);
    if (!check_double_ee(epsilon, 0, 1, str_end)) {
        if (help_str) *help_str = " <epsilon> <detal> <Delta>\n[Error] Invalid epsilon\n";
        return nullptr;
    }
    double delta = std::strtod(argv[argi++], &str_end);
    if (!check_double_ee(delta, 0, 1, str_end)) {
        if (help_str) *help_str = " <epsilon> <detal> <Delta>\n[Error] Invalid delta\n";
        return nullptr;
    }
    double Delta = std::strtod(argv[argi++], &str_end);
    if (!check_double_ee(Delta, 0, INFINITY, str_end)) {
        if (help_str) *help_str = " <epsilon> <detal> <Delta>\n[Error] Invalid Delta\n";
        return nullptr;
    }

    return new PAMSketch(epsilon, delta, Delta);
}

PAMSketch *PAMSketch::get_test_instance() {
    return new PAMSketch(0.01, 0.1, 0.5);
}

PAMSketch *PAMSketch::create_from_config(int idx) {
    double epsilon = g_config->get_double("PAMS.epsilon").value();
    double delta = g_config->get_double("PAMS.delta").value();
    double Delta = g_config->get_double("PAMS.Delta").value();

    return new PAMSketch(epsilon, delta, Delta);
}


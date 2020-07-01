#include "pams.h"
#include "conf.h"
#include <sstream>

using namespace std;

PAMSketch::PAMSketch(double eps, double delta, double Delta,
        uint32_t seed) :
    w(ceil(exp(1)/eps)),
    d(ceil(log(1/delta))),
    D(Delta),
    p_sampling(1/Delta),
    rgen(seed),
    m_eps(eps),
    m_delta(delta),
    m_Delta(Delta),
    m_u32_hash_param() {

    C.resize(d);
    for (unsigned int i = 0; i < d; i++) {
        C[i].resize(w);
    }

    //srand(time(NULL));
    std::uniform_int_distribution<int> unif(0, std::numeric_limits<int>::max());
    ksi.resize(d);
    for (unsigned int i = 0; i < d; i++) {
        ksi[i] = std::make_tuple(
            unif(rgen), unif(rgen), unif(rgen), unif(rgen));
    }

    prepare_u32_hash();
}

void PAMSketch::prepare_u32_hash()
{
    std::uniform_int_distribution<uint64_t> unif(0, (((uint64_t) 1) << 63) - 1);
    m_u32_hash_param.resize(d); 
    for (unsigned int i = 0; i < d; ++i)
    {
        m_u32_hash_param[i].first = unif(rgen);
        m_u32_hash_param[i].second = unif(rgen);
    }
}

void PAMSketch::clear() {
    for (unsigned i = 0; i < d; ++i) {
        C[i].clear();
        C[i].resize(w);
    }
}

void PAMSketch::update(unsigned long long t, const char *str, int c) {
    update_impl(t, str_hash(str), c);
}

void
PAMSketch::update(
    TIMESTAMP ts,
    uint32_t value,
    int c)
{
    update_impl(ts, value, c);
}

void PAMSketch::update_impl(TIMESTAMP ts, uint64_t hashval, int c)
{
    for (unsigned int j = 0; j < d; j++) {
        unsigned int h = u32_hash(j, hashval);
        unsigned int f = flag(j, hashval);
        C[j][h][f].val += c;

        if (p_sampling(rgen)) {
            C[j][h][f].samples.emplace_back(std::make_pair(ts, C[j][h][f].val));
        }
    }

}

double
PAMSketch::estimate_point_in_interval(
    const char *str,
    unsigned long long s,
    unsigned long long e)
{
    return estimate_point_in_interval_impl(
        str_hash(str),
        s,
        e);
}

double
PAMSketch::estimate_point_in_interval_impl(
    uint64_t hashval,
    TIMESTAMP s,
    TIMESTAMP e) const
{
    std::vector<double> D;
    D.reserve(d);
    for (unsigned int j = 0; j < d; j++) {
        unsigned int h = u32_hash(j, hashval);
        int Xi_value = Xi(j, hashval);
        double D_s = Xi_value * (estimate_C(j, h, 1, s) - estimate_C(j, h, 0, s));
        double D_e = Xi_value * (estimate_C(j, h, 1, e) - estimate_C(j, h, 0, e));
        
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

double PAMSketch::estimate_C(unsigned j, unsigned i, unsigned f, unsigned long long t) const {
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

uint64_t
PAMSketch::estimate_frequency(
    TIMESTAMP ts_e,
    uint32_t key) const
{
    return (uint64_t) std::round(
            estimate_point_in_interval_impl(key, 0, ts_e));
}

uint64_t
PAMSketch::estimate_frequency_bitp(
    TIMESTAMP ts_s,
    uint32_t key) const
{
    return (uint64_t) std::round(
            estimate_point_in_interval_impl(key, ts_s, (TIMESTAMP) ~0ul));
}

size_t PAMSketch::memory_usage() const {
    size_t mem = sizeof(*this) + sizeof(double) * m_u32_hash_param.size();
    
    mem += 2 * sizeof(Counter) * C.capacity() * C[0].capacity();
    for (unsigned j = 0; j < d; ++j)
    for (unsigned h = 0; h < w; ++h) {
        mem += sizeof(C[j][h][0].samples.front()) *
            (C[j][h][0].samples.capacity() + C[j][h][1].samples.capacity());
    }

    mem += ksi.capacity() * sizeof(ksi[0]);

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
    
    double epsilon = g_config->get_double("PAMS.epsilon", idx).value();
    double delta = g_config->get_double("PAMS.delta", idx).value();
    double Delta = g_config->get_double("PAMS.Delta", idx).value();
    uint32_t seed = g_config->get_u32("PAMS.seed").value();
    
    return new PAMSketch(epsilon, delta, Delta, seed);
}


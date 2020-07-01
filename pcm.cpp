#include "pcm.h"
#include <cstdlib>
#include "conf.h"
#include <sstream>
#include <random>

using namespace std;

CMSketch::CMSketch(double eps, double delta, uint64_t seed) :
    w(ceil(exp(1)/eps)),
    d(ceil(log(1/delta))),
    m_u32_hash_param() {
    
    std::mt19937 rgen(seed);
    std::uniform_int_distribution<uint64_t> unif(0, (((uint64_t) 1) << 61)- 1);
    C.resize(d);
    m_u32_hash_param.resize(d);
    for (unsigned int i = 0; i < d; i++) {
        C[i].resize(w, 0);
        m_u32_hash_param[i].first = unif(rgen);
        m_u32_hash_param[i].second = unif(rgen);
    }
}

void CMSketch::clear() {
    for (unsigned int i = 0; i < d; i++) {
        fill(C[i].begin(), C[i].end(), 0);
    }
}

void CMSketch::update(const char *str, int c) {
    update_impl(str_hash(str), c);
}

void CMSketch::update_impl(uint64_t hashval, int c)
{
    for (unsigned int j = 0; j < d; j++) {
	    int h = u32_hash(j, hashval);
        C[j][h] += c;
    }
}

uint64_t CMSketch::estimate(const char *str) const {
    return estimate_impl(str_hash(str));
}

uint64_t CMSketch::estimate_impl(uint64_t hashval) const {
    int val = numeric_limits<int>::max();
    for (unsigned int j = 0; j < d; j++) {
	    unsigned int h = u32_hash(j, hashval);
        val = min(val, C[j][h]);
    }
    return val;
}

size_t CMSketch::memory_usage() const {
    return sizeof(int) * C.capacity() * C[0].capacity() + sizeof(*this) +
        m_u32_hash_param.size() * sizeof(m_u32_hash_param[0]);
}

// PCMSketch below
PCMSketch::PCMSketch(double eps, double delta, double Delta, uint64_t seed): 
    CMSketch(eps, delta, seed),
    m_eps(eps), m_delta(delta), m_Delta(Delta) {

    pla.resize(d);
    for (unsigned int i = 0; i < d; i++) {
        pla[i].reserve(w);
        for (unsigned int j = 0; j < w; j++) {
            pla[i].push_back(PLA(Delta));
        }
    }
}

void PCMSketch::clear() {
    CMSketch::clear();
    for (unsigned int i = 0; i < d; i++) {
        for (unsigned int j= 0; j < w; j++) {
            pla[i][j].clear();
        }
    }
}

void PCMSketch::update(unsigned long long t, const char *str, int c) {
    update_impl(t, str_hash(str), c);
}

void
PCMSketch::update(
    TIMESTAMP ts,
    uint32_t key,
    int c)
{
    update_impl(ts, key, c);
}

void PCMSketch::update_impl(TIMESTAMP ts, uint64_t hashval, int c) {
    for (unsigned int j = 0; j < d; j++) {
	    unsigned h = u32_hash(j, hashval);
        C[j][h] += c;
        pla[j][h].feed({ts, (double)C[j][h]});
    }
}
double PCMSketch::estimate_point_in_interval(
    const char *str, unsigned long long s, unsigned long long e) {
    
    return estimate_point_in_interval_impl(
            str_hash(str), s, e);
}

double PCMSketch::estimate_point_in_interval_impl(
    uint64_t hashval, TIMESTAMP ts_s, TIMESTAMP ts_e) const {
    
    double *vals = new double[d];
    for (unsigned int j = 0; j < d; j++) {
        unsigned h = u32_hash(j, hashval);
        const PLA &target = pla[j][h];
        vals[j] = target.estimate(ts_e) - target.estimate(ts_s);
    }
    sort(vals, vals + d);
    double ret;
    if (d & 1) { // d is odd
        ret = vals[d/2];
    } else {
        ret = (vals[d/2] + vals[(d/2)-1]) / 2;
    }
    delete []vals;
    return ret;
}

double PCMSketch::estimate_point_at_the_time(
    const char *str,
    unsigned long long ts_e) {
    
    return estimate_point_in_interval(str, 0, ts_e); 
}

uint64_t
PCMSketch::estimate_frequency(
    TIMESTAMP ts_e,
    uint32_t key) const
{
    return (uint64_t) std::round(
            estimate_point_in_interval_impl(key, 0, ts_e));
}

uint64_t
PCMSketch::estimate_frequency_bitp(
    TIMESTAMP ts_s,
    uint32_t key) const
{
    return (uint64_t) std::round(
            estimate_point_in_interval_impl(key, ts_s, (TIMESTAMP) ~0ul));
}

size_t PCMSketch::memory_usage() const {
    size_t sum = 0;
    for (unsigned int i = 0; i < d; i++) {
        for (unsigned int j = 0; j < w; j++) {
            sum += (size_t) pla[i][j].memory_usage();
        }
    }
    return (size_t) CMSketch::memory_usage() + sum;
}

std::string PCMSketch::get_short_description() const {
    std::ostringstream oss;
    oss << std::fixed << "PCM-e" << m_eps << "-d" << m_delta << "-D" << m_Delta;
    return oss.str();
}

PCMSketch *PCMSketch::create(int &argi, int argc, char *argv[], const char **help_str) {
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

    return new PCMSketch(epsilon, delta, Delta);
}

PCMSketch *PCMSketch::get_test_instance() {
    return new PCMSketch(0.01, 0.1, 0.5);
}

PCMSketch *PCMSketch::create_from_config(int idx) {
    double epsilon = g_config->get_double("PCM.epsilon", idx).value();
    double delta = g_config->get_double("PCM.delta", idx).value();
    double Delta = g_config->get_double("PCM.Delta", idx).value();
    uint32_t seed = g_config->get_u32("PCM.seed").value();

    return new PCMSketch(epsilon, delta, Delta, seed);
}


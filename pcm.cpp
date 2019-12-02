#include "pcm.h"
#include <cstdlib>
#include "conf.h"
#include <sstream>

using namespace std;

CMSketch::CMSketch(double eps, double delta) :
    w(ceil(exp(1)/eps)),
    d(ceil(log(1/delta))) {
    C.resize(d);
    for (unsigned int i = 0; i < d; i++) {
        C[i].resize(w, 0);
    }
    srand(time(NULL));
    hashes.resize(d);
    for (unsigned int i = 0; i < d; i++) {
        hashes[i] = rand_int();
    }
}

void CMSketch::clear() {
    for (unsigned int i = 0; i < d; i++) {
        fill(C[i].begin(), C[i].end(), 0);
    }
}

void CMSketch::update(const char *str, int c) {
    size_t len = strlen(str);
    uint64_t hashval[2];
    for (unsigned int j = 0; j < d; j++) {
        MurmurHash3_x64_128(str, len, hashes[j], hashval);
	int h = (hashval[0] ^ hashval[1]) % w;
        C[j][h] += c;
    }
}

int CMSketch::estimate(const char *str) {
    size_t len = strlen(str);
    int val = numeric_limits<int>::max();
    uint64_t hashval[2];
    for (unsigned int j = 0; j < d; j++) {
        MurmurHash3_x64_128(str, len, hashes[j], hashval);
	int h = (hashval[0] ^ hashval[1]) % w;
        val = min(val, C[j][h]);
    }
    return val;
}

unsigned long long CMSketch::memory_usage() const {
    return d * w * sizeof(int);
}

PCMSketch::PCMSketch(double eps, double delta, double Delta) : CMSketch(eps, delta),
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
    size_t len = strlen(str);
    uint64_t hashval[2];
    for (unsigned int j = 0; j < d; j++) {
        MurmurHash3_x64_128(str, len, hashes[j], hashval);
	int h = (hashval[0] ^ hashval[1]) % w;
        C[j][h] += c;
        pla[j][h].feed({t, (double)C[j][h]});
    }
}

double PCMSketch::estimate_point_in_interval(
    const char *str, unsigned long long s, unsigned long long e) {
    size_t len = strlen(str);
    uint64_t hashval[2];
    double vals[d];
    for (unsigned int j = 0; j < d; j++) {
        MurmurHash3_x64_128(str, len, hashes[j], hashval);
	int h = (hashval[0] ^ hashval[1]) % w;
        PLA &target = pla[j][h];
        vals[j] = target.estimate(e) - target.estimate(s);
    }
    sort(vals, vals + d);
    if (d & 1) { // d is odd
        return vals[d/2];
    } else {
        return (vals[d/2] + vals[(d/2)-1]) / 2;
    }
}

double PCMSketch::estimate_point_at_the_time(
    const char *str,
    unsigned long long ts_e) {
    
    return estimate_point_in_interval(str, 0, ts_e); 
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
    double epsilon = g_config->get_double("PCM.epsilon").value();
    double delta = g_config->get_double("PCM.delta").value();
    double Delta = g_config->get_double("PCM.Delta").value();

    return new PCMSketch(epsilon, delta, Delta);
}


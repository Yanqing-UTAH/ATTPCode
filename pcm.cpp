#include "pcm.h"

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
        hashes[i].first = rand_int();
        hashes[i].second = rand_int();
    }
}

void CMSketch::clear() {
    for (unsigned int i = 0; i < d; i++) {
        fill(C[i].begin(), C[i].end(), 0);
    }
}

void CMSketch::update(const char *str, int c) {
    unsigned int item = hashstr(str);
    for (unsigned int j = 0; j < d; j++) {
        unsigned int hashval = (hashes[j].first * item + hashes[j].second) % w;
        C[j][hashval] += c;
    }
}

int CMSketch::estimate(const char *str) {
    unsigned int item = hashstr(str);
    int val = numeric_limits<int>::max();
    for (unsigned int j = 0; j < d; j++) {
        unsigned int hashval = (hashes[j].first * item + hashes[j].second) % w;
        val = min(val, C[j][hashval]);
    }
    return val;
}

unsigned long long CMSketch::memory_usage() {
    return d * w * sizeof(int);
}

PCMSketch::PCMSketch(double eps, double delta, double Delta) : CMSketch(eps, delta){
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
    unsigned int item = hashstr(str);
    for (unsigned int j = 0; j < d; j++) {
        unsigned int hashval = (hashes[j].first * item + hashes[j].second) % w;
        C[j][hashval] += c;
        pla[j][hashval].feed({t, (double)C[j][hashval]});
    }
}

double PCMSketch::estimate_point_in_interval(
    const char *str, unsigned long long s, unsigned long long e) {
    unsigned int item = hashstr(str);
    double vals[d];
    for (unsigned int j = 0; j < d; j++) {
        unsigned int hashval = (hashes[j].first * item + hashes[j].second) % w;
        PLA &target = pla[j][hashval];
        vals[j] = target.estimate(e) - target.estimate(s);
    }
    sort(vals, vals + d);
    if (d & 1) { // d is odd
        return vals[d/2];
    } else {
        return (vals[d/2] + vals[(d/2)-1]) / 2;
    }
}

size_t PCMSketch::memory_usage() {
    size_t sum = 0;
    for (unsigned int i = 0; i < d; i++) {
        for (unsigned int j = 0; j < w; j++) {
            sum += (size_t) pla[i][j].memory_usage();
        }
    }
    return (size_t) CMSketch::memory_usage() + sum;
}


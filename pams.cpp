#include "pams.h"

using namespace std;

PAMS::PAMS(double eps, double delta) :
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

void PAMS::clear() {
    for (unsigned int i = 0; i < d; i++) {
        fill(C[i].begin(), C[i].end(), 0);
    }
}

void PAMS::update(const char *str, int c) {
    int item = hashstr(str);
    for (unsigned int j = 0; j < d; j++) {
        unsigned int hashval = (hashes[j].first * item + hashes[j].second) % w;
        char flag = (flags[j].first * item + flags[j].second) % 2;
        C[j][hashval][flag] += c;
    }
}

int PAMS::estimate(const char *str) {
    int item = hashstr(str);
    int val = numeric_limits<int>::max();

    for (unsigned int j = 0; j < d; j++) {
        unsigned int hashval = (hashes[j].first * item + hashes[j].second) % w;
        val = min(val, C[j][hashval]);
    }
    return val;
}

size_t PAMS::memory_usage() {
    return d * w * sizeof(int);
}

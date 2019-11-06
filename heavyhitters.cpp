#include "heavyhitters.h"

using namespace std;

HeavyHitters::HeavyHitters(unsigned logUniverseSize) : levels(logUniverseSize) {
	assert(logUniverseSize > 0);
	pcm = new PCMSketch*[levels];
	for (auto i = 0; i < levels; ++i) {
		pcm[i] = new PCMSketch(0.0005, 0.1, 0.1);
	}
}

HeavyHitters::~HeavyHitters() {
	for (auto i = 0; i < levels; ++i) {
		delete pcm[i];
	}
	delete [] pcm;
}

void HeavyHitters::update(unsigned long long ts, unsigned element, int cnt) {
	unsigned idx = element;
	char buffer[30];
	for (auto i = 0; i < levels; ++i) {
		sprintf(buffer, "%u", idx);
		pcm[i]->update(ts, buffer, cnt);
		idx >>= 1;
	}
}

vector<unsigned long long> HeavyHitters::query_hh(unsigned long long ts, double threshold) const {
	vector<unsigned long long> result;
	vector<pair<int, int>> stack = {{levels - 1, 0U}, {levels - 1, 1U}};
	char buffer[30];
	while (!stack.empty()) {
		auto [level, x] = stack.back();
		stack.pop_back();
		sprintf(buffer, "%u", x);
		auto freq = pcm[level]->estimate_point_at_the_time(buffer, ts);
		if (freq >= threshold) {
			if (level == 0) {
				result.push_back(x);
			}
			else {
				stack.push_back({level - 1, x << 1});
				stack.push_back({level - 1, (x << 1) | 1});
			}
		}
	}
	return result;
}

size_t HeavyHitters::memory_usage() const {
	size_t s = 0;
	for (int i = 0; i < levels; ++i) {
		s += pcm[i]->memory_usage();
	}
	return s;
}

#include "heavyhitters.h"
#include <sstream>
#include "conf.h"
#include <iostream>

using namespace std;

HeavyHitters::HeavyHitters(unsigned logUniverseSize) : levels(logUniverseSize) {
	assert(logUniverseSize > 0);
	pcm = new PCMSketch*[levels];
	for (auto i = 0; i < levels; ++i) {
		pcm[i] = new PCMSketch(0.0005, 0.1, 0.1);
	}
    cnt_pla = nullptr;
}

HeavyHitters::HeavyHitters(
    unsigned logUniverseSize,
    double epsilon,
    double delta,
    double Delta):
    levels(logUniverseSize),
    pcm(nullptr),
    tot_cnt(0ull),
    cnt_pla(nullptr),
    m_eps(epsilon),
    m_delta(delta),
    m_Delta(Delta)
{
    pcm = new PCMSketch*[levels];
    for (auto i = 0; i < levels; ++i)
    {
        pcm[i] = new PCMSketch(epsilon, delta, Delta);
    }
    cnt_pla = new PLA(Delta);
}

HeavyHitters::~HeavyHitters() {
	for (auto i = 0; i < levels; ++i) {
		delete pcm[i];
	}
	delete [] pcm;
    delete cnt_pla;
}

void HeavyHitters::clear() {
    for (auto i = 0; i < levels; ++i)
    {
        pcm[i]->clear();
    }
    if (cnt_pla)
    {
        cnt_pla->clear();
        tot_cnt = 0;
    }
}

void HeavyHitters::update(unsigned long long ts, uint32_t element, int cnt) {
	unsigned idx = element;
	char buffer[30];
	for (auto i = 0; i < levels; ++i) {
		sprintf(buffer, "%u", idx);
		pcm[i]->update(ts, buffer, cnt);
		idx >>= 1;
	}
    
    if (cnt_pla)
    {
        ++tot_cnt;
        cnt_pla->feed(PLA::point{ts, (double) tot_cnt});
    }
}

vector<uint32_t> HeavyHitters::query_hh(unsigned long long ts, double threshold) const {
	vector<uint32_t> result;
	vector<pair<int, unsigned>> stack = {{levels - 1, 0U}, {levels - 1, 1U}};
	char buffer[30];
    //uint64_t n = 0;
	while (!stack.empty()) {
		auto [level, x] = stack.back();
		stack.pop_back();
		sprintf(buffer, "%u", x);
        //if (++n % 100000 == 0)
            //cout << stack.size() << endl;
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
	return std::move(result);
}

std::vector<IPersistentHeavyHitterSketch::HeavyHitter>
HeavyHitters::estimate_heavy_hitters(
    TIMESTAMP ts_e,
    double frac_threshold) const
{
    double cnt_est = cnt_pla->estimate(ts_e);
    double threshold = frac_threshold * cnt_est;
    
    //decltype(query_hh(ts_e, threshold)) raw_result;
    auto raw_result = query_hh(ts_e, threshold);
    
    std::vector<IPersistentHeavyHitterSketch::HeavyHitter> ret;
    std::transform(raw_result.begin(), raw_result.end(),
        std::back_inserter(ret),
        [ts_e,this,cnt_est](uint32_t value) -> auto {
            char buffer[30];
            sprintf(buffer, "%u", value);
            auto freq = pcm[0]->estimate_point_at_the_time(buffer, ts_e);
            return IPersistentHeavyHitterSketch::HeavyHitter{
                .m_value = value,
                .m_fraction = (float) (freq / cnt_est)
            };
        });
    return std::move(ret); 
}

size_t HeavyHitters::memory_usage() const {
	size_t s = 0;
	for (int i = 0; i < levels; ++i) {
		s += pcm[i]->memory_usage();
	}
    if (cnt_pla) s += cnt_pla->memory_usage();
	return s;
}

std::string HeavyHitters::get_short_description() const {
    std::ostringstream oss;
    oss << "PCM_HH-logU" << levels << "-e" << m_eps << "-d" << m_delta << "-D" << m_Delta;
    return oss.str();
}

HeavyHitters*
HeavyHitters::create(int &argi, int argc, char *argv[], const char **help_str)
{
    if (argi + 4 > argc)
    {
        if (help_str) *help_str = " <logUniverseSize> <epsilon> <delta> <Delta>\n";
        return nullptr;
    }

    char *str_end;
    auto v = strtol(argv[argi++], &str_end, 0);
    if (!check_long_ii(v, 1, 32, str_end))
    {
        if (help_str) *help_str = " <logUniverseSize> <epsilon> <delta> <Delta>\n[Error] logUniverseSize must be between 1 and 32\n" ;
        return nullptr;
    }
    unsigned logUniverseSize = (unsigned) v;

    double epsilon = std::strtod(argv[argi++], &str_end);
    if (!check_double_ee(epsilon, 0, 1, str_end)) {
        if (help_str) *help_str = " <logUniverseSize> <epsilon> <detal> <Delta>\n[Error] Invalid epsilon\n";
        return nullptr;
    }
    double delta = std::strtod(argv[argi++], &str_end);
    if (!check_double_ee(delta, 0, 1, str_end)) {
        if (help_str) *help_str = " <logUniverseSize> <epsilon> <detal> <Delta>\n[Error] Invalid delta\n";
        return nullptr;
    }
    double Delta = std::strtod(argv[argi++], &str_end);
    if (!check_double_ee(Delta, 0, INFINITY, str_end)) {
        if (help_str) *help_str = " <logUniverseSize> <epsilon> <detal> <Delta>\n[Error] Invalid Delta\n";
        return nullptr;
    }

    return new HeavyHitters(logUniverseSize, epsilon, delta, Delta);
}

HeavyHitters*
HeavyHitters::get_test_instance()
{
    return new HeavyHitters(5, 0.01, 0.1, 0.5);
}

HeavyHitters*
HeavyHitters::create_from_config(int idx)
{
    uint32_t logUniverseSize = g_config->get_u32("PCM_HH.log_universe_size", idx).value();
    double epsilon = g_config->get_double("PCM_HH.epsilon", idx).value();
    double delta = g_config->get_double("PCM_HH.delta", idx).value();
    double Delta = g_config->get_double("PCM_HH.Delta", idx).value();

    return new HeavyHitters(logUniverseSize, epsilon, delta, Delta);
}

int
HeavyHitters::num_configs_defined()
{
    if (g_config->is_list("PCM_HH.log_universe_size"))
    {
        int len = (int) g_config->list_length("PCM_HH.log_universe_size");
        if (len != g_config->list_length("PCM_HH.epsilon") ||
            len != g_config->list_length("PCM_HH.delta") ||
            len != g_config->list_length("PCM_HH.Delta"))
        {
            std::cerr << "[WARN] PCM_HH ignored because list length mismatch in params"
                << std::endl;
            return 0;
        }
        return len;
    }

    return -1;
}


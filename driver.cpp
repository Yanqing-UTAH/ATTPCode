#include <iostream>
#include <cstring>
#include <unordered_map>
#include <set>
#include <sstream>
#include <fstream>
#include "sketch.h"

template<class T>
struct ResourceGuard {
    ResourceGuard(T *t) {
        m_t = t;
    }
    ~ResourceGuard() {
        delete m_t;
    }

private:
    T *m_t;
};

using namespace std;

/*  const char *data[] = {
    "hello", "some", "one", "hello", "alice",
    "one", "lady", "let", "us", "lady",
    "alice", "in", "wonderland", "us", "lady",
    "lady", "some", "hello", "none", "pie"

  };

int count(int begin, int end, const char * str) {
    int cnt = 0;
    for (int i = begin; i <= end; ++i) {
        if (!strcmp(str, data[i])) {
            cnt += 1;
        }
    }
    return cnt;
}

void pcm_test(PCMSketch *pcm, const char *str, int begin, int end) {
    cout << str << "[" << begin << "," << end << "]:\tEst: " << pcm->estimate_point_in_interval(str, begin, end) << "\tTruth: " << count(begin, end, str) << endl; 
} */

void print_help(const char *progname, const char *help_str = nullptr) {
    cout << "usage: " << progname << " <QueryType> <SketchType> <infile> <params...>";
    if (help_str) cout << help_str;
    cout << endl;

    cout << "available query types:" << endl;
    cout << "\tpoint_interval" << endl;
    cout << "\tpoint_att (TODO)" << endl;
    cout << "available sketch types:" << endl;
    cout << "\tPCM (persistent_count_min)" << endl;
    cout << "\tPAMS (persistent_AMS_Sketch)" << endl;
}

std::tuple<unsigned long long, unsigned long long, std::string>
parse_point_interval_query(const std::string &line) {
    std::istringstream in(line);

    char c;
    unsigned long long ts_s, ts_e;
    std::string str;
    in >> c >> ts_s >> ts_e >> str;
    return std::make_tuple(ts_s, ts_e, str);
}

const char*
test_point_interval(
    const char *sketch_name,
    const char *infile_name,
    int argc, char *argv[]) {
    
    SKETCH_TYPE st = sketch_name_to_sketch_type(sketch_name);
    if (st == ST_INVALID) {
        return "\n[ERROR] Unknown sketch type";
    }

    const char *help_str;
    IPersistentPointQueryable *ippq =
        create_persistent_point_queryable(st, argc, argv, &help_str);
    if (!ippq) return help_str;

    ResourceGuard guard(ippq);
    
    std::ifstream infile(infile_name); 
    if (!infile) {
        return "\n[ERROR] Unable to open input file";
    }

    std::unordered_map<std::string, std::set<unsigned long long>> cnt;
    
    unsigned long long ts = 0;
    std::string line;
    while (std::getline(infile, line), !line.empty()) {
        if (line[0] == '?') {
            unsigned long long ts_s, ts_e;
            std::string str;
            std::tie(ts_s, ts_e, str) = parse_point_interval_query(line);
            
            double est_value = ippq->estimate_point_in_interval(str.c_str(), ts_s, ts_e);
            unsigned long long true_value = 0;
            auto &ts_set = cnt[str];
            for (auto iter = ts_set.upper_bound(ts_s);
                    iter != ts_set.end() && *iter <= ts_e;
                    ++iter) ++true_value;

            cout << str << "[" << ts_s << "," << ts_e << "]:\tEst: "
                << est_value << "\tTruth: " <<  true_value << endl; 
        } else {
            ippq->update(++ts, line.c_str());
            cnt[line].emplace(ts);
        } // TODO turnstile: prefix the input with '-'
    }

    cout << "memory_usage() == " << ippq->memory_usage() << endl;

    return nullptr;
}

int main(int argc, char **argv) {

    setup_sketch_lib();

    const char *progname = argv[0];
    
    if (argc < 4) {
        print_help(progname);
        return 1;
    }

    const char *query_type = argv[1];
    const char *sketch_type = argv[2];
    const char *infile_name = argv[3];

    if (!strcmp(query_type, "point_interval")) {
        const char *help_str =
            test_point_interval(sketch_type, infile_name, argc - 4, argv + 4);
        if (help_str) {
            print_help(progname, help_str);
            return 2; 
        }
    } else if (!strcmp(query_type, "point_att")) {
        print_help(progname);
        return 1;
    } else {
        print_help(progname);
        return 1;
    }

    return 0;
}


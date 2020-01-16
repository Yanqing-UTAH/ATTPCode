#include <iostream>
#include <cstring>
#include <unordered_map>
#include <set>
#include <sstream>
#include <fstream>
#include "sketch.h"
#include <cassert>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctime>
#include <iomanip>

using namespace std;

/* old impl. */
void print_help(const char *progname, const char *help_str = nullptr) {
    cout << "usage: " << progname << " <QueryType> <SketchType> <infile>";
    if (help_str)
    {
        cout << help_str; 
    }
    else
    {
        /* We have argc <= 2 at this point */
        cout << " <params...>";
        cout << endl;
        cout << "Hint: enter query type to view the list of supported sketch types" << endl;
        cout << "\navailable query types:" << endl;
        cout << "\tpoint_interval" << endl;
        cout << "\tpoint_att" << endl;
        cout << "\theavy_hitter" << endl;
    }
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
    IPersistentSketch *sketch,
    const char *infile_name) {

    IPersistentPointQueryable *ippq = dynamic_cast<IPersistentPointQueryable*>(sketch);
    assert(ippq);
    
    std::ifstream infile(infile_name); 
    if (!infile) {
        return "\n[ERROR] Unable to open input file\n";
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

std::tuple<unsigned long long, std::string>
parse_point_att_query(const std::string &line) {
    std::istringstream in(line);

    char c;
    unsigned long long ts_e;
    std::string str;
    in >> c >> ts_e >> str;
    return std::make_tuple(ts_e, str);
}

const char*
test_point_att(
    IPersistentSketch *sketch,
    const char *infile_name) {

    IPersistentPointQueryable *ippq = dynamic_cast<IPersistentPointQueryable*>(sketch);
    assert(ippq);
    
    
    std::ifstream infile(infile_name);
    if (!infile) {
        return "\n[ERROR] Unable to open input file\n";
    }

    std::unordered_map<std::string, std::set<unsigned long long>> cnt;

    unsigned long long ts = 0;
    std::string line;
    while (std::getline(infile, line), !line.empty()) {
        if (line[0] == '?') {
            unsigned long long ts_e;
            std::string str;
            std::tie(ts_e, str) = parse_point_att_query(line);
            
            double est_value = ippq->estimate_point_at_the_time(str.c_str(), ts_e);
            unsigned long long true_value = 0;
            auto &ts_set = cnt[str];
            for (auto iter = ts_set.begin();
                    iter != ts_set.end() && *iter <= ts_e;
                    ++iter) ++true_value;

            cout << str << "[0," << ts_e << "]:\tEst: "
                << est_value << "\tTruth: " <<  true_value << endl; 
        } else {
            ippq->update(++ts, line.c_str());
            cnt[line].emplace(ts);
        } // TODO turnstile: prefix the input with '-'
    }

    cout << "memory_usage() == " << ippq->memory_usage() << endl;

    return nullptr;
}

const char*
test_heavy_hitter(
    IPersistentSketch *sketch,
    const char *infile_name)
{
    IPersistentHeavyHitterSketch *iphh =
        dynamic_cast<IPersistentHeavyHitterSketch*>(sketch);
    assert(iphh);

    std::ifstream infile(infile_name);
    if (!infile)
    {
        return "\n[ERROR] Unable to open input file\n";
    }
    
    std::string line;
    while (std::getline(infile, line), !line.empty()) {
        if (line[0] == '?')
        {
            TIMESTAMP ts_e;
            double fraction;
            sscanf(line.c_str(), "? %llu %lf", &ts_e, &fraction);
            auto estimated = iphh->estimate_heavy_hitters(ts_e, fraction);
            cout << "HeavyHitter(" << fraction << "|" << ts_e << ") = {" << endl;
            for (const auto &hh: estimated)
            {
                struct in_addr ip = { .s_addr = (in_addr_t) hh.m_value };
                
                cout << '\t' << inet_ntoa(ip) << ' ' << hh.m_fraction << endl;
            }
            cout << '}' << endl;
        }
        else
        {
            TIMESTAMP ts;
            char ip_str[17];
            struct in_addr ip;

            sscanf(line.c_str(), "%llu %16s", &ts, ip_str);
            if (!inet_aton(ip_str, &ip))
            {
                cout << "[WARN] Malformatted line: " << line << endl;
                continue;
            }
            iphh->update(ts, (uint32_t) ip.s_addr);
        }
    }

    return nullptr;
}

int old_main(int argc, char **argv) {

    setup_sketch_lib();

    int argi = 0;
    const char *progname = argv[argi++];
    if (argc < 2) {
        print_help(progname);
        return 1;
    }

    const char *help_str = nullptr;
    const char *query_type = argv[argi++];
    std::vector<SKETCH_TYPE> supported_sketch_types =
        check_query_type(query_type, &help_str);
    if (help_str)
    {
        print_help(progname, help_str);
        return 1;
    }

    auto get_supported_sketch_types_helpstr= [&](ssize_t &off) -> const char *
    {

        if (off >= help_str_bufsize)
        {
            help_str_buffer[help_str_bufsize - 1] = '\0';
            return help_str_buffer;
        }

        off += snprintf(help_str_buffer + off, help_str_bufsize - off,
            "\nSupported sketch types for %s:\n", query_type);
        if (off >= help_str_bufsize)
        {
            help_str_buffer[help_str_bufsize - 1] = '\0';
            return help_str_buffer;
        }

        for (auto st: supported_sketch_types)
        {
            off += snprintf(help_str_buffer + off, help_str_bufsize - off,
                "\t%s (%s)\n", sketch_type_to_sketch_name(st),
                sketch_type_to_altname(st));
            if (off >= help_str_bufsize)
            {
                help_str_buffer[help_str_bufsize - 1] = '\0';
                break;
            }
        }
        return help_str_buffer; 
    };

    if (argc < 3)
    {
        ssize_t off = snprintf(help_str_buffer, help_str_bufsize,
            " <params...>\n");
        help_str = get_supported_sketch_types_helpstr(off);
        print_help(progname, help_str);
        return 1;
    }
    
    const char *sketch_name = argv[argi++];
    SKETCH_TYPE st = sketch_name_to_sketch_type(sketch_name);
    if (st == ST_INVALID) {
        ssize_t off = snprintf(help_str_buffer, help_str_bufsize,
            " <params...>\n[ERROR] Unknown sketch type: %s\n", sketch_name);
        help_str = get_supported_sketch_types_helpstr(off);
        print_help(progname, help_str);
        return 1;
    }

    if (supported_sketch_types.end() == 
        std::find(supported_sketch_types.begin(), supported_sketch_types.end(), st))
    {
        ssize_t off = snprintf(help_str_buffer, help_str_bufsize,
            " <params...>\n[ERROR] Unsupported sketch type: %s\n", sketch_name);
        help_str = get_supported_sketch_types_helpstr(off);
        print_help(progname, help_str);
        return 1;
    }

    if (argi >= argc)
    {
        print_help(progname, " <params...>\n[ERROR] missing input file\n");
        return 1;
    }
    const char *infile_name = argv[argi++];

    IPersistentSketch *sketch =
        create_persistent_sketch(st, argi, argc, argv, &help_str);
    if (!sketch) {
        print_help(progname, help_str);
        return 1;
    }
    ResourceGuard guard(sketch);

    help_str = nullptr; 
    if (!strcmp(query_type, "point_interval")) {
        help_str =
            test_point_interval(sketch, infile_name);
    } else if (!strcmp(query_type, "point_att")) {
        help_str =
            test_point_att(sketch, infile_name);
    } else if (!strcmp(query_type, "heavy_hitter")) {
        help_str =
            test_heavy_hitter(sketch, infile_name);
    } else {
        print_help(progname);
        return 1;
    }
    
    if (help_str)
    {
        print_help(progname, help_str);
        return 2;
    }
    return 0;
}

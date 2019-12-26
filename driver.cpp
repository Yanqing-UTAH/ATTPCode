#include <iostream>
#include <cstring>
#include <unordered_map>
#include <set>
#include <sstream>
#include <fstream>
#include "sketch.h"
#include <cassert>
#include <algorithm>
#include "util.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctime>
#include "conf.h"
#include "misra_gries.h"
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

std::string format_outfile_name(
    const std::string &outfile_name,
    const char *time_text,
    IPersistentHeavyHitterSketch *sketch)
{
    std::string ret;
    ret.reserve(outfile_name.length() + 256);
    for (std::string::size_type i = 0; i < outfile_name.length(); ++i)
    {
        if (outfile_name[i] == '%' && i + 1 < outfile_name.length())
        {
            switch (outfile_name[++i])
            {
            case 'T':
                ret.append(time_text);
                break;
            case 's':
                ret.append(sketch->get_short_description());
                break;
            default:
                ret.push_back('%');
                --i;
            }
        }
        else
        {
            ret.push_back(outfile_name[i]);
        }
    }
    
    return std::move(ret);
}

/* new impl. */
int run_new_heavy_hitter(bool is_bitp = false)
{
    std::time_t tt = std::time(nullptr);
    std::tm local_time = *std::localtime(&tt);
    char text_tt[256];
    if (0 == strftime(text_tt, 256, "%Y/%m/%d %H:%M:%S", &local_time))
    {
        strcpy(text_tt, "<time_buffer_too_small>");
    }

    auto input_type = g_config->get("HH.input_type").value();
    bool input_is_ip;
    if (input_type == "IP")
    {
        input_is_ip = true;
    }
    else if (input_type == "uint32")
    {
        input_is_ip = false;
    }
    else
    {
        std::cerr << "[ERROR] Invalid HH.input_type: " << input_type
             << " (IP or uint32 required)" << std::endl;
        return 1;
    }

    std::cerr << "Running query heavy_hitter at " << text_tt << std::endl;
    if (0 == strftime(text_tt, 256, "%Y-%m-%d-%H-%M-%S", &local_time))
    {
        strcpy(text_tt, "<time_buffer_too_small>");
    }

    std::vector<SKETCH_TYPE> supported_sketch_types =
        !is_bitp ?
        check_query_type("heavy_hitter", nullptr) :
        check_query_type("heavy_hitter_bitp", nullptr);
    
    int exact_pos = -1;
    std::vector<ResourceGuard<IPersistentHeavyHitterSketch>> sketches;
    for (unsigned i = 0; i < supported_sketch_types.size(); ++i)
    {
        auto st = supported_sketch_types[i]; 
        if (g_config->get_boolean(
                std::string(sketch_type_to_sketch_name(st)) + ".enabled").value_or(false))
        {
            if (!strcmp(sketch_type_to_sketch_name(st), "EXACT_HH"))
            {
                exact_pos = (int) sketches.size();
            }
            
            auto added_sketches = create_persistent_sketch_from_config(st);
            for (auto &sketch: added_sketches)
            {
                sketches.emplace_back(dynamic_cast<IPersistentHeavyHitterSketch*>(sketch));
            }
        }
    }
    
    std::string infile_name = g_config->get("infile").value();
    std::optional<std::string> outfile_name_opt = g_config->get("outfile");
    uint64_t out_limit = g_config->get_u64("out_limit").value();

    std::ifstream infile(infile_name);
    if (!infile)
    {
        std::cerr << "[ERROR] Unable to open input file" << std::endl;
        return 1;
    }
    
    const bool has_outfile = (bool) outfile_name_opt;
    std::vector<ResourceGuard<std::ostream>> outfiles;
    if (has_outfile)
    {
        for (auto &rg_iphh: sketches)
        {
            outfiles.emplace_back(new std::ofstream(format_outfile_name(
                outfile_name_opt.value(), text_tt, rg_iphh.get())));
        }
    }

    std::string line;
    size_t n_data = 0;
    while (std::getline(infile, line))
    {
        if (line[0] == '?')
        {
            TIMESTAMP ts_e;
            double fraction;
            // for BITP, ts_e is actually ts_s, but the input format
            // is the same
            sscanf(line.c_str(), "? %llu %lf", &ts_e, &fraction);

            std::cout << "HH(" << fraction << "|" << ts_e <<"): " << std::endl;
            
            std::vector<IPersistentHeavyHitterSketch::HeavyHitter> exact_answer;
            std::unordered_set<uint32_t> exact_answer_set;
            if (exact_pos != -1)
            {
                exact_answer = sketches[exact_pos].get()->estimate_heavy_hitters(
                        ts_e, fraction);
                std::transform(exact_answer.begin(), exact_answer.end(),
                        std::inserter(exact_answer_set, exact_answer_set.end()),
                        [](const auto &hh) -> uint32_t {
                            return hh.m_value;
                        });
                std::cout << "\t" << sketches[exact_pos].get()->get_short_description()
                    << ':' << exact_answer.size() << std::endl;
            }

            for (int i = 0; i < (int) sketches.size(); ++i)
            {
                std::vector<IPersistentHeavyHitterSketch::HeavyHitter> answer;

                if (i != exact_pos)
                {
                    answer = sketches[i].get()->estimate_heavy_hitters(
                        ts_e, fraction);
                    
                    if (exact_pos != -1)
                    {
                        size_t intersection_count = 
                        std::count_if(answer.begin(), answer.end(),
                            [&exact_answer_set](auto &hh) -> bool {
                                return exact_answer_set.find(hh.m_value) != exact_answer_set.end();
                            });

                        double precision = (double) intersection_count / answer.size();
                        double recall = (double) intersection_count / exact_answer_set.size();

                        std::cout << '\t'
                            << sketches[i].get()->get_short_description()
                            << ": prec = "
                            << intersection_count << '/' << answer.size()
                            << " = " << precision
                            << ", recall = "
                            << intersection_count << '/' << exact_answer.size()
                            << " = " << recall
                            << std::endl;
                    }
                }

                if (has_outfile)
                {
                    auto &res = (i == exact_pos) ? exact_answer : answer;
                    
                    *outfiles[i].get() << "HH(" << fraction << "|" << ts_e << ") = {" << std::endl;

                    uint64_t n_written = 0;
                    for (const auto &hh: res)
                    {
                        *outfiles[i].get() << '\t';
                        if (input_is_ip)
                        {
                            struct in_addr ip = { .s_addr = (in_addr_t) hh.m_value };
                            
                            *outfiles[i].get() << inet_ntoa(ip); 
                        }
                        else
                        {
                            *outfiles[i].get() << hh.m_value;
                        }
                        *outfiles[i].get() << ' ' << hh.m_fraction << std::endl;
                        if (out_limit > 0 && ++n_written == out_limit)
                        {
                            *outfiles[i].get() << "... <" 
                                << res.size() - n_written
                                << " omitted>"
                                << std::endl;
                        }
                    }

                    *outfiles[i].get() << '}' << std::endl;
                }
            }
        }
        else
        {
            TIMESTAMP ts;
            uint32_t value;
            
            if (input_is_ip)
            {
                char ip_str[17];
                struct in_addr ip;
                sscanf(line.c_str(), "%llu %16s", &ts, ip_str);
                if (!inet_aton(ip_str, &ip))
                {
                    cerr << "[WARN] Malformatted line: " << line << endl;
                    continue;
                }
                value = (uint32_t) ip.s_addr;
            }
            else
            {
                sscanf(line.c_str(), "%llu %u", &ts, &value);
            }
            for (auto &rg_ipph: sketches)
            {
                rg_ipph.get()->update(ts, value);
            }

            ++n_data;
            if (n_data % 50000 == 0)
            {
                std::cout << "Progress: " << n_data << " data points processed"
                    << std::endl;
            }
        }
    }
    
    std::cout << "memory_usage() = " << std::endl;
    for (auto &rg_ipph: sketches)
    {
        size_t mm_b = rg_ipph.get()->memory_usage();
        double mm_mb = mm_b / 1024.0 / 1024;
        char saved_fill = std::cout.fill();
        std::cout << '\t'
             << rg_ipph.get()->get_short_description()
             << ": "
             << rg_ipph.get()->memory_usage()
             << " B = "
             << (size_t) std::floor(mm_mb) << '.'
             << std::setfill('0')
             << ((size_t)(std::floor(mm_mb * 1000))) % 1000
             << std::setfill(saved_fill)
             << " MB"
             << std::endl;
    }

    return 0;
}

void print_new_help(const char *progname) {
    if (progname)
    {
        std::cerr<< "usage: " << progname << " run <ConfigFile>" << std::endl;
        std::cerr<< "usage: " << progname << " help <QueryType>" << std::endl;
    }
    std::cerr << "Available query types:" << std::endl;
    std::cerr << "\theavy_hitter" << std::endl;
    std::cerr << "\theavy_hitter_bitp" << std::endl;
}

int
main(int argc, char *argv[])
{
    if (argc > 1)
    {
        if (!strcmp(argv[1], "--run-old"))
        {
            argv[1] = argv[0];
            return old_main(argc - 1, argv + 1);
        }
        else if (!strcmp(argv[1], "--test-misra-gries"))
        {
            argv[1] = argv[0];
            return MisraGries::unit_test(argc - 1, argv + 1);
        }
    }

    setup_sketch_lib();
    setup_config();

    int argi = 0;
    const char *progname = argv[argi++];
    if (argc < 3)
    {
        print_new_help(progname);
        return 1;
    }
    
    const char *command = argv[argi++];
    if (!strcmp(command, "run"))
    {
        const char *config_file = argv[argi++];
        const char *help_str;
        if (!g_config->parse_file(config_file, &help_str))
        {
            std::cerr << help_str; 
            return 2;
        }
        
        std::string query_type = g_config->get("test_name").value();
        if (query_type == "heavy_hitter")
        {
            return run_new_heavy_hitter();
        }
        else if (query_type == "heavy_hitter_bitp")
        {
            // is_bitp == true
            return run_new_heavy_hitter(true);
        }
        else
        {
            std::cerr << "[ERROR] Invalid query type " << query_type << std::endl;
            print_new_help(nullptr);
            return 1;
        }
    }
    else if (!strcmp(command, "help"))
    {
        const char *query_type = argv[argi++];
        if (!strcmp(query_type, "heavy_hitter"))
        {
            std::cerr << "Query heavy_hitter" << std::endl; 
        }
        else if (!strcmp(query_type, "heavy_hitter_bitp"))
        {
            std::cerr << "Query heavy_hitter_bitp" << std::endl;
        }
        else
        {
            std::cerr << "[ERROR] Invalid query type " << query_type << std::endl;
            print_new_help(nullptr);
            return 1;
        }

        std::cerr << "Supported sketches:" << std::endl;
        std::vector<SKETCH_TYPE> supported_sketch_types =
            check_query_type(query_type, nullptr);
        for (SKETCH_TYPE st: supported_sketch_types)
        {
            std::cerr << '\t' << sketch_type_to_sketch_name(st) << " ("
                << sketch_type_to_altname(st) << ')' << std::endl;
        }
    }
    else
    {
        print_new_help(progname);
        return 1;
    }

    return 0;
}


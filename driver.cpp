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
#include "perf_timer.h"

//using namespace std;

/* old impl. moved to old_driver.cpp */
int old_main(int argc, char *argv[]);

/* new impl. */
std::string format_outfile_name(
    const std::string &outfile_name,
    const char *time_text,
    IPersistentSketch *sketch)
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

#define PERF_TIMER_TIMEIT(timer, action) \
    do { \
        if (!do_measure_time) { action } else { \
            (timer)->measure_start(); \
            { action } \
            (timer)->measure_end();   \
        } \
    } while (0)


int run_new_heavy_hitter()
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

    std::vector<SKETCH_TYPE> supported_sketch_types =
        check_query_type("heavy_hitter", nullptr);
    
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

    std::vector<PerfTimer> update_timers;
    std::vector<PerfTimer> query_timers;
    bool do_measure_time = g_config->get_boolean("perf.measure_time").value();
    if (do_measure_time)
    {
        update_timers.resize(sketches.size());
        query_timers.resize(sketches.size());
    }

    std::cerr << "Running query heavy_hitter at " << text_tt << std::endl;
    if (0 == strftime(text_tt, 256, "%Y-%m-%d-%H-%M-%S", &local_time))
    {
        strcpy(text_tt, "<time_buffer_too_small>");
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
    size_t dbg_break_point = ~0ull;
    while (std::getline(infile, line))
    {
        if (line[0] == '?')
        {
            TIMESTAMP ts_e;
            double fraction;
            sscanf(line.c_str(), "? %llu %lf", &ts_e, &fraction);

            std::cout << "HH(" << fraction << "|" << ts_e <<"): " << std::endl;
            
            std::vector<IPersistentHeavyHitterSketch::HeavyHitter> exact_answer;
            std::unordered_set<uint32_t> exact_answer_set;
            if (exact_pos != -1)
            {
                PERF_TIMER_TIMEIT(&query_timers[exact_pos],
                {
                    exact_answer = 
                        sketches[exact_pos].get()->estimate_heavy_hitters(
                            ts_e, fraction);
                });
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
                    PERF_TIMER_TIMEIT(&query_timers[i],
                    {
                        answer = sketches[i].get()->estimate_heavy_hitters(
                            ts_e, fraction);
                    });
                    
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
        else if (line[0] != '#') // a line starting with # is ignored
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
                    std::cerr << "[WARN] Malformatted line: " << line << std::endl;
                    continue;
                }
                value = (uint32_t) ip.s_addr;
            }
            else
            {
                sscanf(line.c_str(), "%llu %u", &ts, &value);
            }

            for (auto i = 0u; i < sketches.size(); ++i)
            {
                if (n_data == dbg_break_point)
                {
                    sketches[i].get()->update(ts, value);
                    continue;
                }
                PERF_TIMER_TIMEIT(&update_timers[i],
                {
                    sketches[i].get()->update(ts, value);
                });
            }

            ++n_data;
            if (n_data % 50000 == 0)
            {
                std::cout << "Progress: " << n_data << " data points processed"
                    << std::endl;
            }
        }
    }
    
    std::cout << std::endl;
    std::cout << "=============  Memory Usage  =============" << std::endl;
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

    if (do_measure_time)
    {
        std::cout << "=============  Time stats    =============" << std::endl;

        std::cout << "Update timers:" << std::endl;
        for (auto i = 0u; i < sketches.size(); ++i)
        {
            std::cout << '\t'
                << sketches[i].get()->get_short_description()
                << ": tot = "
                << update_timers[i].get_elapsed_ms() << " ms = "
                << update_timers[i].get_elapsed_s() << " s (avg "
                << update_timers[i].get_avg_elapsed_us() << " us = "
                << update_timers[i].get_avg_elapsed_ms() << " ms)"
                << std::endl;
        }
        std::cout << "Query timers:" << std::endl;
        for (auto i = 0u; i < sketches.size(); ++i)
        {
            std::cout << '\t'
                << sketches[i].get()->get_short_description()
                << ": tot = "
                << query_timers[i].get_elapsed_ms() << " ms = "
                << query_timers[i].get_elapsed_s() << " s (avg "
                << query_timers[i].get_avg_elapsed_us() << " us = "
                << query_timers[i].get_avg_elapsed_ms() << " ms)"
                << std::endl;
        }
    }

    return 0;
}

/* new impl. */
int run_new_heavy_hitter_bitp()
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
        check_query_type("heavy_hitter_bitp", nullptr);
    
    int exact_pos = -1;
    std::vector<ResourceGuard<IPersistentHeavyHitterSketchBITP>> sketches;
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
                sketches.emplace_back(dynamic_cast<IPersistentHeavyHitterSketchBITP*>(sketch));
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
            sscanf(line.c_str(), "? %llu %lf", &ts_e, &fraction);

            std::cout << "HH(" << fraction << "|" << ts_e <<"): " << std::endl;
            
            std::vector<IPersistentHeavyHitterSketchBITP::HeavyHitter> exact_answer;
            std::unordered_set<uint32_t> exact_answer_set;
            if (exact_pos != -1)
            {
                exact_answer = 
                    sketches[exact_pos].get()->estimate_heavy_hitters_bitp(
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
                std::vector<IPersistentHeavyHitterSketchBITP::HeavyHitter> answer;

                if (i != exact_pos)
                {
                    answer =
                        sketches[i].get()->estimate_heavy_hitters_bitp(
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
                    std::cerr << "[WARN] Malformatted line: " << line << std::endl;
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
             << mm_b
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
            return run_new_heavy_hitter_bitp();
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


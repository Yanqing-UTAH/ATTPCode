#ifndef QUERY_H
#define QUERY_H

#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <ctime>
#include <cstring>
#include <unordered_map>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "util.h"
#include "conf.h"
#include "sketch.h"
#include "perf_timer.h"

template<
    class ISketchT>
class QueryBase
{
//private:
    //std::ofstream               m_file_out;

protected:
    QueryBase():
        m_out(std::cout)
    {}

    int
    early_setup() { return 0; }

    typedef ISketchT            ISketch; 

    std::vector<ResourceGuard<ISketch>>
                                m_sketches;

    std::ostream                &m_out;

};

template<
    class QueryImpl>
class Query: public QueryImpl
{
protected:
    using typename QueryImpl::ISketch;
    using QueryImpl::m_out;
    using QueryImpl::m_sketches;

public:
    Query():
        QueryImpl(),
        m_measure_time(false),
        m_update_timers(),
        m_query_timers()
    {}

    ~Query()
    {}
    
    int
    setup()
    {
        std::time_t tt = std::time(nullptr);
        std::tm local_time = *std::localtime(&tt);
        char text_tt[256];
        if (0 == strftime(text_tt, 256, "%Y/%m/%d %H:%M:%S", &local_time))
        {
            strcpy(text_tt, "<time_buffer_too_small>");
        }

        m_out << "Running query " 
              << QueryImpl::get_name()
              << " at "
              << text_tt
              << std::endl;
        
        int ret;
        if ((ret = QueryImpl::early_setup()))
        {
            return ret; 
        }

        std::vector<SKETCH_TYPE> supported_sketch_types =
            check_query_type(QueryImpl::get_name(), nullptr);

        for (unsigned i = 0; i < supported_sketch_types.size(); ++i)
        {
            auto st = supported_sketch_types[i];
            if (g_config->get_boolean(
                    std::string(sketch_type_to_sketch_name(st))
                    + ".enabled").value_or(false))
            {
                auto added_sketches = create_persistent_sketch_from_config(st);
                for (auto &sketch: added_sketches)
                {
                    m_sketches.emplace_back(dynamic_cast<ISketch*>(sketch));
                }
            }
        }
        
        m_measure_time = g_config->get_boolean("perf.measure_time").value();
        if (m_measure_time)
        {
            m_update_timers.resize(m_sketches.size());
            m_query_timers.resize(m_sketches.size());
        }

        m_infile_name = g_config->get("infile").value();
        m_infile.open(m_infile_name);

        std::optional<std::string> outfile_name_opt = g_config->get("outfile");
        m_has_outfile = (bool) outfile_name_opt;
        if (m_has_outfile)
        {
            m_outfile_name_template = outfile_name_opt.value();
            for (auto &sketch: m_sketches)
            {
                m_outfiles.emplace_back(new std::ofstream(
                    format_outfile_name(m_outfile_name_template,
                        text_tt, sketch.get())));
            }
        }

        m_out_limit = g_config->get_u64("out_limit").value();
    
        return QueryImpl::additional_setup();
    }

#define PERF_TIMER_TIMEIT(timer, action) \
    do { \
        if (!m_measure_time) { action } else { \
            (timer)->measure_start(); \
            { action } \
            (timer)->measure_end();   \
        } \
    } while (0)

    int
    run()
    {
        if (!m_infile.is_open()) return 1;
        
        std::string line;
        size_t n_data = 0;
        while (std::getline(m_infile, line))
        {
            if (line.empty())
            {
                continue;
            }

            if (line[0] == '?')
            {
                // a query
                TIMESTAMP ts;
                char *pc_arg_start;
                
                ts = (TIMESTAMP) strtoull(line.c_str() + 2, &pc_arg_start, 0);
                if (QueryImpl::parse_query_arg(ts, pc_arg_start))
                {
                    std::cerr << "[WARN] Malformatted line: " << line << std::endl; 
                    continue;
                }

                for (int i = 0; i < (int) m_sketches.size(); ++i)
                {
                    PERF_TIMER_TIMEIT(&m_query_timers[i],
                        QueryImpl::query(m_sketches[i].get(), ts););
                    QueryImpl::print_query_summary(m_sketches[i].get());
                    if (m_has_outfile)
                    {
                        QueryImpl::dump_query_result(m_sketches[i].get(),
                            *m_outfiles[i].get(),
                            ts,
                            m_out_limit);
                    }
                }
            }
            else if (line[0] != '#')
            {
                // a data point
                TIMESTAMP ts;
                char *pc_arg_start;

                ts = (TIMESTAMP) strtoull(line.c_str(), &pc_arg_start, 0);
                if (QueryImpl::parse_update_arg(pc_arg_start))
                {
                    std::cerr << "[WARN] Malformatted line: " << line << std::endl; 
                    continue;
                }

                for (int i = 0; i < (int) m_sketches.size(); ++i)
                {
                    PERF_TIMER_TIMEIT(&m_update_timers[i],
                        QueryImpl::update(m_sketches[i].get(), ts););
                }

                ++n_data;
                if (n_data % 50000 == 0)
                {
                    std::cerr << "Progress: " << n_data << " data points processed"
                        << std::endl;
                }
            }
            // a line starting with # is a comment
        }

        return 0;
    }

#undef PERF_TIMER_TIMEIT

    int
    print_stats()
    {
        m_out << std::endl;
        m_out << "=============  Memory Usage  =============" << std::endl;
        for (auto &sketch: m_sketches)
        {
            size_t mm_b = sketch.get()->memory_usage();
            double mm_mb = mm_b / 1024.0 / 1024;
            char saved_fill = m_out.fill();
            m_out << '\t'
                 << sketch.get()->get_short_description()
                 << ": "
                 << sketch.get()->memory_usage()
                 << " B = "
                 << (size_t) std::floor(mm_mb) << '.'
                 << std::setfill('0')
                 << ((size_t)(std::floor(mm_mb * 1000))) % 1000
                 << std::setfill(saved_fill)
                 << " MB"
                 << std::endl;
        }

        if (m_measure_time)
        {
            m_out << "=============  Time stats    =============" << std::endl;

            m_out << "Update timers:" << std::endl;
            for (auto i = 0u; i < m_sketches.size(); ++i)
            {
                m_out << '\t'
                    << m_sketches[i].get()->get_short_description()
                    << ": tot = "
                    << m_update_timers[i].get_elapsed_ms() << " ms = "
                    << m_update_timers[i].get_elapsed_s() << " s (avg "
                    << m_update_timers[i].get_avg_elapsed_us() << " us = "
                    << m_update_timers[i].get_avg_elapsed_ms() << " ms)"
                    << std::endl;
            }
            m_out << "Query timers:" << std::endl;
            for (auto i = 0u; i < m_sketches.size(); ++i)
            {
                m_out << '\t'
                    << m_sketches[i].get()->get_short_description()
                    << ": tot = "
                    << m_query_timers[i].get_elapsed_ms() << " ms = "
                    << m_query_timers[i].get_elapsed_s() << " s (avg "
                    << m_query_timers[i].get_avg_elapsed_us() << " us = "
                    << m_query_timers[i].get_avg_elapsed_ms() << " ms)"
                    << std::endl;
            }
        }

        return 0;
    }

private:
    static std::string format_outfile_name(
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

    bool                        m_measure_time,
                                
                                m_has_outfile;

    std::vector<PerfTimer>      m_update_timers;

    std::vector<PerfTimer>      m_query_timers;

    std::string                 m_infile_name;

    std::string                 m_outfile_name_template;

    std::ifstream               m_infile;

    std::vector<ResourceGuard<std::ostream>>
                                m_outfiles;

    uint64_t                    m_out_limit;
};

////////////////////////////////////////
// Heavy hitter query implementation  //
////////////////////////////////////////

template<class IHHSketch>
struct HHSketchQueryHelper
{};

template<>
struct HHSketchQueryHelper<IPersistentHeavyHitterSketch>
{
    static std::vector<HeavyHitter_u32>
    estimate(
        IPersistentHeavyHitterSketch *sketch,
        TIMESTAMP ts_e,
        double frac_threshold)
    {
        return sketch->estimate_heavy_hitters(ts_e, frac_threshold);
    }
};

template<>
struct HHSketchQueryHelper<IPersistentHeavyHitterSketchBITP>
{
    static std::vector<HeavyHitter_u32>
    estimate(
        IPersistentHeavyHitterSketchBITP *sketch,
        TIMESTAMP ts_s,
        double frac_threshold)
    {
        return sketch->estimate_heavy_hitters_bitp(ts_s, frac_threshold);
    }
};

template<class IHHSketch>
class QueryHeavyHitterImpl:
    public QueryBase<IHHSketch>
{
protected:
    using QueryBase<IHHSketch>::m_out;
    using QueryBase<IHHSketch>::m_sketches;

    const char *
    get_name() const
    {
        return IHHSketch::query_type;
    }

    int 
    additional_setup()
    {
        auto input_type = g_config->get("HH.input_type").value();
        if (input_type == "IP")
        {
            m_input_is_ip = true;
        }
        else if (input_type == "uint32")
        {
            m_input_is_ip = false;
        }
        else
        {
            std::cerr << "[ERROR] Invalid HH.input_type: " << input_type
                 << " (IP or uint32 required)" << std::endl;
            return 1;
        }

        if (g_config->get_boolean("EXACT_HH.enabled").value())
        {
            m_exact_enabled = true; 
            if (m_sketches.size() == 0 ||
                m_sketches[0].get()->get_short_description() != "EXACT_HH")
            {
                std::cerr << "[ERROR] exact query should come first" << std::endl;
                return 1;
            }
        }
        else
        {
            m_exact_enabled = false;
        }

        return 0;
    }

    int
    parse_query_arg(
        TIMESTAMP ts,
        const char *str)
    {
        m_query_fraction = strtod(str, nullptr);
        m_out << "HH(" << m_query_fraction << "|"  << ts << "):" << std::endl;
        return 0;
    }

    void
    query(
        IHHSketch *sketch,
        TIMESTAMP ts)
    {
        m_last_answer = HHSketchQueryHelper<IHHSketch>::estimate(
            sketch, ts, m_query_fraction);
    }

    void
    print_query_summary(
        IHHSketch *sketch)
    {
        if (m_exact_enabled && sketch == m_sketches[0].get())
        {
            m_exact_answer_set.clear();
            std::transform(
                m_last_answer.begin(),
                m_last_answer.end(),
                std::inserter(m_exact_answer_set, m_exact_answer_set.end()),
                [](const auto &hh) -> uint32_t {
                    return hh.m_value;
                });

            m_out << '\t'
                << sketch->get_short_description()
                << ": "
                << m_exact_answer_set.size()
                << std::endl;
        }
        else
        {
            if (m_exact_enabled && sketch != m_sketches[0].get())
            {
                size_t intersection_count = 
                    std::count_if(m_last_answer.begin(),
                        m_last_answer.end(),
                        [this](auto &hh) -> bool {
                            return m_exact_answer_set.find(hh.m_value) != m_exact_answer_set.end();
                        });
                double prec = (double) intersection_count / m_last_answer.size();
                double recall = (double) intersection_count / m_exact_answer_set.size();

                m_out << '\t'
                    << sketch->get_short_description()
                    << ": prec = "
                    << intersection_count
                    << '/'
                    << m_last_answer.size()
                    << " = "
                    << prec
                    << ", recall = "
                    << intersection_count
                    << '/'
                    << m_exact_answer_set.size()
                    << " = "
                    << recall
                    << std::endl;
            }
        }
    }

    void
    dump_query_result(
        IHHSketch *sketch,
        std::ostream &out,
        TIMESTAMP ts,
        uint64_t out_limit)
    {
        out << "HH(" << m_query_fraction << '|' << ts << ") = {" << std::endl;
        uint64_t n_written = 0;
        for (const auto &hh: m_last_answer)
        {
            out << '\t';
            if (m_input_is_ip)
            {
                struct in_addr ip = { .s_addr = (in_addr_t) hh.m_value };
                out << inet_ntoa(ip);
            }
            else
            {
                out << hh.m_value;
            }
            out << ' ' << hh.m_fraction << std::endl;
            if (out_limit > 0 && ++n_written == out_limit)
            {
                out << "... <"
                    << m_last_answer.size() - n_written
                    << " omitted>"
                    << std::endl;
            }
        }
        out << '}' << std::endl;
    }

    int
    parse_update_arg(
        const char *str)
    {
        if (m_input_is_ip)
        {
            while (std::isspace(*str)) ++str;
            char ip_str[17];
            strncpy(ip_str, str, 16);
            ip_str[16] = '\0';
        
            struct in_addr ip;
            if (!inet_aton(ip_str, &ip))
            {
                return 1;
            }
            m_update_value = (uint32_t) ip.s_addr;
        }
        else
        {
            m_update_value = (uint32_t) strtoul(str, nullptr, 0);
        }

        return 0;
    }

    void
    update(
        IHHSketch *sketch,
        TIMESTAMP ts)
    {
        sketch->update(ts, m_update_value);
    }

private:
    bool                        m_input_is_ip;

    bool                        m_exact_enabled;

    uint32_t                    m_update_value;

    double                      m_query_fraction;

    std::vector<HeavyHitter_u32>
                                m_last_answer;

    std::unordered_set<uint32_t>
                                m_exact_answer_set;
};

using QueryHeavyHitter = Query<QueryHeavyHitterImpl<IPersistentHeavyHitterSketch>>;
using QueryHeavyHitterBITP = Query<QueryHeavyHitterImpl<IPersistentHeavyHitterSketchBITP>>;

////////////////////////////////////////
// Matrix sketch query implementation //
////////////////////////////////////////
class QueryMatrixSketchImpl:
    public QueryBase<IPersistentMatrixSketch>
{
protected:
    QueryMatrixSketchImpl();

    ~QueryMatrixSketchImpl();

    constexpr const char *
    get_name() const
    { return "matrix_sketch"; }

    int
    early_setup();

    int
    additional_setup();
    
    int
    parse_query_arg(
        TIMESTAMP ts,
        const char *str);

    void
    query(
        IPersistentMatrixSketch *sketch,
        TIMESTAMP ts);

    void
    print_query_summary(
        IPersistentMatrixSketch *sketch);

    void
    dump_query_result(
        IPersistentMatrixSketch *sketch,
        std::ostream &out,
        TIMESTAMP ts,
        uint64_t out_limit);

    int
    parse_update_arg(
        const char *str);

    void
    update(
        IPersistentMatrixSketch *sketch,
        TIMESTAMP ts);

private:
    inline
    size_t
    matrix_size() const
    {
        return (size_t) m_n * ((size_t) m_n + 1) / 2;
    }

    bool                        m_exact_enabled;

    int                         m_n;

    double                      *m_dvec; // next input vec

    double                      *m_last_answer; // upper triangle matrix

    double                      *m_exact_covariance_matrix; // upper triangle matrix

    double                      *m_work; // of size m_n * m_n

    double                      *m_singular_values;

    double                      m_exact_fnorm;
};

using QueryMatrixSketch = Query<QueryMatrixSketchImpl>;

#endif // QUEYR_H


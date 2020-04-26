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
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstdio>
#include <thread>
#include <atomic>
#include <cassert>
#include "util.h"
#include "conf.h"
#include "sketch.h"
#include "perf_timer.h"
extern "C"
{
#include <lapacke.h>
}
#include "lapack_wrapper.h"


////////////////////////////////////////
//          Query template            //
////////////////////////////////////////

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
        m_query_timers(),
        m_progress_bar_stopped(true),
        m_progress_bar_status(PBS_NONE)
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

        m_suppress_progress_bar = g_config->get_boolean("misc.suppress_progress_bar").value();
        
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
        m_n_data = 0;
        m_infile_read_bytes = 0;
    
        struct stat infile_stat;
        stat(m_infile_name.c_str(), &infile_stat);
        m_infile_tot_bytes = infile_stat.st_size;

        m_stderr_is_a_tty = isatty(fileno(stderr));
    
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

        start_progress_bar();
        
        std::string line;
        size_t lineno = 0;
        while (std::getline(m_infile, line))
        {
            ++lineno;
            m_infile_read_bytes = m_infile.tellg();
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
                    fprintf(stderr,
                        "[WARN] malformatted line on %lu\n",
                        (uint64_t) lineno);
                    continue;
                }

                pause_progress_bar();

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
                
                if (m_stderr_is_a_tty)
                {
                    m_out << std::endl;
                }
                continue_progress_bar();
            }
            else if (line[0] != '#')
            {
                // a data point
                TIMESTAMP ts;
                char *pc_arg_start;

                ts = (TIMESTAMP) strtoull(line.c_str(), &pc_arg_start, 0);
                if (QueryImpl::parse_update_arg(pc_arg_start))
                {
                    fprintf(stderr,
                        "[WARN] malformatted line on %lu\n",
                        (uint64_t) lineno);
                    continue;
                }

                for (int i = 0; i < (int) m_sketches.size(); ++i)
                {
                    PERF_TIMER_TIMEIT(&m_update_timers[i],
                        QueryImpl::update(m_sketches[i].get(), ts););
                }

                ++m_n_data;
                /*if (m_n_data % 50000 == 0)
                {
                    std::cerr << "Progress: " << m_n_data << " data points processed"
                        << std::endl;
                } */
            }
            // a line starting with # is a comment
        }

        stop_progress_bar();

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
                 << mm_b
                 << " B = "
                 << (size_t) std::floor(mm_mb) << '.'
                 << std::setfill('0')
                 << std::setw(3)
                 << ((size_t)(std::floor(mm_mb * 1000))) % 1000
                 << std::setfill(saved_fill)
                 << std::setw(0)
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
    static std::string
    format_outfile_name(
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
    
    void
    print_progress_bar_content()
    {
        uint64_t n_data = *(volatile uint64_t *) &m_n_data;
        uint64_t read_bytes = *(volatile uint64_t *) &m_infile_read_bytes;
        if (m_stderr_is_a_tty)
        {
            fprintf(stderr,
                "\033[A\033[2K\rProgress: %lu dp processed (approx. %.2f%%)\n",
                n_data, (double) 100 * read_bytes / m_infile_tot_bytes);
        }
        else
        {
            fprintf(stderr,
                "Progress: %lu dp processed (approx. %.2f%%)\n",
                n_data, (double) 100 * read_bytes / m_infile_tot_bytes);
        }
        fflush(stderr);
    }

    void
    show_progress_bar()
    {
        const std::chrono::duration interval =
            m_stderr_is_a_tty ? std::chrono::milliseconds(500) :
            std::chrono::seconds(10); // fall back to write one line at a time if
                                      // stderr is not a terminal and we do not want
                                      // to write too much progress reports as well

        m_progress_bar_status.store(PBS_WAITING, std::memory_order_relaxed); 
        while (!*(volatile bool *) &m_progress_bar_stopped)
        {
            std::this_thread::sleep_for(interval);
            
            ProgressBarStatus s = PBS_WAITING;
            if (!m_progress_bar_status.compare_exchange_strong(
                    s, PBS_REFRESHING, std::memory_order_relaxed))
            {
                continue;
            }

            // report the current progress
            print_progress_bar_content();

            m_progress_bar_status.store(PBS_WAITING, std::memory_order_relaxed);
        }

        m_progress_bar_status.store(PBS_NONE);
    }
    
    void
    start_progress_bar()
    {
        if (m_suppress_progress_bar) return;
        fprintf(stderr, "\n");
        m_progress_bar_stopped = false;
        m_progress_bar_thread = std::thread(&Query<QueryImpl>::show_progress_bar, this);
    }

    void
    pause_progress_bar()
    {
        if (m_suppress_progress_bar) return;

        ProgressBarStatus s = PBS_WAITING;
        while (!m_progress_bar_status.compare_exchange_strong(
                    s, PBS_PAUSED, std::memory_order_relaxed))
        {
            if (s == PBS_NONE) return ;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            s = PBS_WAITING;
        }
    }

    void
    continue_progress_bar()
    {
        if (m_suppress_progress_bar) return;

        fprintf(stderr, "\n");
        ProgressBarStatus s = PBS_PAUSED;
        (void) m_progress_bar_status.compare_exchange_strong(
            s, PBS_WAITING, std::memory_order_relaxed);
        assert(s == PBS_PAUSED || s == PBS_NONE);
    }

    void
    stop_progress_bar()
    {
        if (m_suppress_progress_bar) return;

        m_progress_bar_stopped = true;
        m_progress_bar_thread.join();
        print_progress_bar_content();
    }

    bool                        m_measure_time,
                                
                                m_has_outfile,

                                m_stderr_is_a_tty,

                                m_suppress_progress_bar;

    std::vector<PerfTimer>      m_update_timers;

    std::vector<PerfTimer>      m_query_timers;

    std::string                 m_infile_name;

    std::string                 m_outfile_name_template;

    std::ifstream               m_infile;

    std::vector<ResourceGuard<std::ostream>>
                                m_outfiles;

    uint64_t                    m_out_limit;

    uint64_t                    m_n_data;

    uint64_t                    m_infile_tot_bytes;

    uint64_t                    m_infile_read_bytes;

    std::thread                 m_progress_bar_thread;
    
    bool                        m_progress_bar_stopped;

    enum ProgressBarStatus {
        PBS_NONE = 0,
        PBS_WAITING = 1,
        PBS_REFRESHING = 2,
        PBS_PAUSED = 3
    };

    std::atomic<ProgressBarStatus>
                                m_progress_bar_status;
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
            fprintf(stderr,
                "[ERROR] Invalid HH.input_type: %s (IP or uint32 required)\n",
                input_type.c_str());
            return 1;
        }

        if (g_config->get_boolean("EXACT_HH.enabled").value())
        {
            m_exact_enabled = true; 
            if (m_sketches.size() == 0 ||
                m_sketches[0].get()->get_short_description() != "EXACT_HH")
            {
                fprintf(stderr,
                    "[ERROR] exact query should be the first in the sketch list");
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



////////////////////////////////////////
// Matrix sketch query implementation //
////////////////////////////////////////
QueryMatrixSketchImpl::QueryMatrixSketchImpl():
    m_exact_enabled(false),
    m_n(0),
    m_dvec(nullptr),
    m_last_answer(nullptr),
    m_exact_covariance_matrix(nullptr),
    m_work(nullptr),
    m_singular_values(nullptr),
    m_exact_fnorm(0)
{}

QueryMatrixSketchImpl::~QueryMatrixSketchImpl()
{
    delete []m_dvec;
    delete []m_last_answer;
    delete []m_exact_covariance_matrix;
    delete []m_work;
    delete []m_singular_values;
}

int
QueryMatrixSketchImpl::early_setup()
{
    if (g_config->is_assigned("MS.dimension"))
    {
        m_n = g_config->get_u32("MS.dimension").value();
    }
    else
    {
        // infer m_n from the input
        std::string infile = g_config->get("infile").value();
        std::ifstream fin(infile);
        if (!fin) return 1;

        std::string line;
        m_n = 0;
        while (std::getline(fin, line))
        {
            if (line.empty()) continue;
            if (line[0] == '?' || line[0] == '#') continue;

            m_n = 0;
            const char *s = line.c_str();
            while (*s && !std::isspace(*s)) ++s;
            for (;*s;)
            {
                while (std::isspace(*s)) ++s;
                if (*s)
                {
                    ++m_n;
                    while (*s && !std::isspace(*s)) ++s;
                }
            }
            break;
        }

        if (m_n == 0)
        {
            std::cerr << "[ERROR] Unable to determine matrix dimension" << std::endl;
            return 1;
        }

        g_config->set_u32("MS.dimension", m_n);
    }
    
    m_dvec = new double[m_n];
    m_last_answer = new double[matrix_size()];
    m_exact_covariance_matrix = new double[matrix_size()];
    m_work = new double[m_n * m_n];
    m_singular_values = new double[m_n];

    return 0;
}

int
QueryMatrixSketchImpl::additional_setup()
{

    if (g_config->get_boolean("EXACT_MS.enabled").value())
    {
        m_exact_enabled = true;
        if (m_sketches.size() == 0 ||
            m_sketches[0].get()->get_short_description() != "EXACT_MS")
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
QueryMatrixSketchImpl::parse_query_arg(
    TIMESTAMP ts,
    const char *str)
{
    m_out << "MS(" << ts << "):" << std::endl;
    return 0;
}

void
QueryMatrixSketchImpl::query(
    IPersistentMatrixSketch *sketch,
    TIMESTAMP ts)
{
    sketch->get_covariance_matrix(ts, m_last_answer);
}

void
QueryMatrixSketchImpl::print_query_summary(
    IPersistentMatrixSketch *sketch)
{
    if (!m_exact_enabled) return ;
    if (m_exact_enabled && sketch == m_sketches[0].get())
    {
        std::swap(m_last_answer, m_exact_covariance_matrix);
        m_exact_fnorm = lapack_wrapper_dlansp('f', 'u', m_n, m_exact_covariance_matrix);
        m_out << "\tF-norm = " << m_exact_fnorm << std::endl;
    }
    else
    {
        // expand the packed form to a general matrix
        int k = 0;
        for (int j = 0; j < m_n; ++j)
        {
            for (int i = 0; i < j; ++i)
            {
                m_work[i * m_n + j] = 
                m_work[j * m_n + i] = m_last_answer[k] - m_exact_covariance_matrix[k];
                ++k;
            }
            m_work[j * m_n + j] = m_last_answer[k] - m_exact_covariance_matrix[k];
            ++k;
        }

        // do svd
        (void) LAPACKE_dgesdd(
            LAPACK_COL_MAJOR,
            'N',
            m_n,
            m_n,
            m_work,
            m_n,
            m_singular_values,
            nullptr,
            1,
            nullptr,
            1);

        m_out << '\t'
            << sketch->get_short_description()
            << ": "
            << "||ATA-BTB||_2 / ||A||_F^2 = "
            << m_singular_values[0] / (m_exact_fnorm * m_exact_fnorm)
            //<< ' '
            //<< m_singular_values[0]
            << std::endl;
    }
}

void
QueryMatrixSketchImpl::dump_query_result(
    IPersistentMatrixSketch *sketch,
    std::ostream &out,
    TIMESTAMP ts,
    uint64_t out_limit)
{
    // TODO do something
}

int
QueryMatrixSketchImpl::parse_update_arg(
    const char *str)
{
    const char *s = str;
    for (int i = 0; i < m_n; ++i)
    {
        char *s2;
        m_dvec[i] = strtod(s, &s2);
        if (s2 == s || (m_dvec[i] == HUGE_VAL)) return 1;
        s = s2;
    }
    
    return 0;
}

void
QueryMatrixSketchImpl::update(
    IPersistentMatrixSketch *sketch,
    TIMESTAMP ts)
{
    sketch->update(ts, m_dvec);
}

////////////////////////////////////////
//      Query public interface        //
////////////////////////////////////////

static std::vector<std::string> supported_query_types({
    "heavy_hitter",
    "heavy_hitter_bitp",
    "matrix_sketch"
});

bool
is_supported_query_type(
    std::string query_type)
{
    
    return std::find(supported_query_types.begin(),
            supported_query_types.end(),
            query_type) != supported_query_types.end();
}

const std::vector<std::string>&
get_supported_query_type_list()
{
    return supported_query_types;
}

template<class Q>
int run_query_impl()
{
    Q query;
    int ret;

    if ((ret = query.setup())) return ret;
    if ((ret = query.run())) return ret;
    if ((ret = query.print_stats())) return ret;

    return 0;
}

using QueryHeavyHitter = Query<QueryHeavyHitterImpl<IPersistentHeavyHitterSketch>>;
using QueryHeavyHitterBITP = Query<QueryHeavyHitterImpl<IPersistentHeavyHitterSketchBITP>>;
using QueryMatrixSketch = Query<QueryMatrixSketchImpl>;

int
run_query(
    std::string query_type)
{
    if (query_type == "heavy_hitter")
    {
        return run_query_impl<QueryHeavyHitter>();
        //return run_new_heavy_hitter();
    }
    else if (query_type == "heavy_hitter_bitp")
    {
        return run_query_impl<QueryHeavyHitterBITP>();
    }
    else if (query_type == "matrix_sketch")
    {
        return run_query_impl<QueryMatrixSketch>();
    }
    
    return 2;
}


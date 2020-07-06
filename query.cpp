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
#include <random>
#include "util.h"
#include "conf.h"
#include "sketch.h"
#include "perf_timer.h"
extern "C"
{
#include <cblas.h>
#include <lapacke.h>
}
#include "lapack_wrapper.h"
#include <fftw3.h>


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

    void finish() {}

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
        m_progress_bar_status(PBS_NONE),
        m_infile_last_read_bytes(0),
        m_last_progress_bar_tp(),
        m_current_avg_rate_s(0)
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
    
        // This is for the output file path.
        // We can't use \/ and shouln't use ' ' or ':' in file name.
        if (0 == strftime(text_tt, 256, "%Y-%m-%d-%H-%M-%S", &local_time))
        {
            strcpy(text_tt, "<time_buffer_too_small>");
        }
        
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
                auto file_name = format_outfile_name(
                    m_outfile_name_template,
                    text_tt,
                    sketch.get());
                m_outfiles.emplace_back(new std::ofstream(file_name));
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
            else if (line[0] == '+')
            {
                // stat request
                pause_progress_bar();
                m_out << "Stats request at line " << lineno
                    << " with " << m_n_data << " processed" << std::endl;
                print_stats();
                m_out << std::endl;
                continue_progress_bar();
            }
            else if (line[0] != '#')
            {
                // a data point
                TIMESTAMP ts;
                char *pc_arg_start;

                ts = (TIMESTAMP) strtoull(line.c_str(), &pc_arg_start, 0);
                if (QueryImpl::parse_update_arg(ts, pc_arg_start))
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

            if (sketch->max_memory_usage_overriden())
            {
                size_t max_mm_b = sketch->max_memory_usage();
                double max_mm_mb = max_mm_b / 1024.0 / 1024;
                char saved_fill = m_out.fill();
                m_out << '\t'
                     << sketch.get()->get_short_description()
                     << "_max: "
                     << max_mm_b
                     << " B = "
                     << (size_t) std::floor(max_mm_mb) << '.'
                     << std::setfill('0')
                     << std::setw(3)
                     << ((size_t)(std::floor(max_mm_mb * 1000))) % 1000
                     << std::setfill(saved_fill)
                     << std::setw(0)
                     << " MB"
                     << std::endl;
            }
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

    void
    finish()
    {
        QueryImpl::finish();
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
    
        auto tp = std::chrono::steady_clock::now();
        uint64_t rem_bytes = m_infile_tot_bytes - read_bytes;
        uint64_t delta_bytes = read_bytes - m_infile_last_read_bytes;
        uint64_t rate_s;
        if (delta_bytes == 0 || m_last_progress_bar_tp == tp)
        {
            rate_s = 0;
        }
        else
        {
            double ms_elapsed = 
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    tp - m_last_progress_bar_tp).count();
            double rate_ms = delta_bytes / ms_elapsed;
            rate_s = (uint64_t) (rate_ms * 1000);
        }
        if (m_infile_last_read_bytes != 0)
        {
            m_current_avg_rate_s = (rate_s + 1 + m_current_avg_rate_s) / 2;
        }
        else
        {
            m_current_avg_rate_s = rate_s;
        }
        

        if (m_stderr_is_a_tty)
        {
            fprintf(stderr, "\033[A\033[2K\r");
        }
        fprintf(stderr,
            "Progress: %lu dp processed (approx. %.2f%%), rate = %lu bytes/s",
            n_data, (double) 100 * read_bytes / m_infile_tot_bytes,
            m_current_avg_rate_s);
        if (m_current_avg_rate_s == 0)
        {
            fprintf(stderr, ", ETA: unknown\n");
        }
        else
        {
            fprintf(stderr, ", ETA: %lu s\n",
                (uint64_t)(rem_bytes * 1.0 / m_current_avg_rate_s));
        }
        fflush(stderr);

        m_last_progress_bar_tp = tp;
        m_infile_last_read_bytes = read_bytes;
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
        m_infile_last_read_bytes = 0;
        m_last_progress_bar_tp = std::chrono::steady_clock::now();
        m_current_avg_rate_s = 0;
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

    uint64_t                    m_infile_last_read_bytes;

    std::chrono::steady_clock::time_point
                                m_last_progress_bar_tp;

    uint64_t                    m_current_avg_rate_s;
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
        out << "#" << sketch->get_short_description() << std::endl;
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
        TIMESTAMP ts,
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
        TIMESTAMP ts,
        const char *str);

    void
    update(
        IPersistentMatrixSketch *sketch,
        TIMESTAMP ts);

    void
    finish();

private:
    inline
    size_t
    matrix_size() const
    {
        return (size_t) m_n * ((size_t) m_n + 1) / 2;
    }

    void
    print_query_summary_with_exact_answer(
        IPersistentMatrixSketch *sketch);
    
    // overwrites m_work, m_ae_V, m_ae_VT_hat, m_singular_values
    void
    print_query_summary_with_analytic_error(
        IPersistentMatrixSketch *sketch);

    void
    print_query_summary_common(
        IPersistentMatrixSketch *sketch,
        double err) const;

    double
    get_exact_fnorm_sqr(
        TIMESTAMP ts) const;
    
    // overwrites m_work, m_ae_work2
    void
    analytic_error_mul_ata_btb(
        double *V, /* length == m_n, stride = m_ae_K, in and out */
        double *VT_hat, /* in */
        double *sig_hat_sqr /* in */) const;

    static fftw_plan
    get_dct_plan(
        int n,
        double *in,
        int istride,
        double *out,
        unsigned flags);

    bool                        m_exact_enabled;
    
    bool                        m_use_analytic_error;

    int                         m_n;

    double                      *m_dvec; // next input vec

    double                      *m_last_answer; // upper triangle matrix

    double                      *m_exact_covariance_matrix; // upper triangle matrix

    double                      *m_work; // of size m_n * m_n

    double                      *m_singular_values;
    
    // Now removed.
    //double                      m_exact_fnorm_sqr;
    
    int                         m_num_dirs;
    
    bool                        m_has_dir_list;
    
    // non-null only when m_use_analytic_error
    // length == m_num_dirs
    int                         *m_num_vecs;
    
    // non-null only when m_use_analytic_error
    // contains m_num_dirs counts of double arrays
    // each of size m_num_vecs[k]
    double                      **m_var_list;
    
    // non-null only when m_has_dir_list (and m_use_analytic_error)
    // contains m_num_dirs counts of double arrays
    // m_dir_list[k] is a column-major array of size m_num_vecs[k] by m_n
    double                      **m_dir_list;
   
    // populated only when m_use_analytic_error
    // each pair is a timestamp and an array of length m_num_dirs
    std::vector<std::pair<TIMESTAMP, uint64_t*>>
                                m_cnt_maps;

    TIMESTAMP                   m_current_query_ts;

    std::vector<std::pair<TIMESTAMP, double>>
                                m_exact_fnorm_sqr_vec;
    
    static constexpr int        m_ae_K = 10;

    static constexpr int        m_ae_P = 100;

    // m_ae_K rows by m_n columns
    // column-major
    double                      *m_ae_V;
    
    // m_n by m_n
    // column-major
    double                      *m_ae_VT_hat;
    
    // length m_n
    double                      *m_ae_work2;

    fftw_plan                   m_fftw_plan2;
};


////////////////////////////////////////
// Matrix sketch query implementation //
////////////////////////////////////////
QueryMatrixSketchImpl::QueryMatrixSketchImpl():
    m_exact_enabled(false),
    m_use_analytic_error(false),
    m_n(0),
    m_dvec(nullptr),
    m_last_answer(nullptr),
    m_exact_covariance_matrix(nullptr),
    m_work(nullptr),
    m_singular_values(nullptr),
    //m_exact_fnorm_sqr(0),
    m_num_dirs(0),
    m_has_dir_list(false),
    m_num_vecs(nullptr),
    m_var_list(nullptr),
    m_dir_list(nullptr),
    m_cnt_maps(),
    m_current_query_ts(0),
    m_exact_fnorm_sqr_vec(),
    m_ae_V(nullptr),
    m_ae_VT_hat(nullptr),
    m_ae_work2(nullptr),
    m_fftw_plan2(nullptr)
{
    m_exact_fnorm_sqr_vec.emplace_back(0, 0.0);
}

QueryMatrixSketchImpl::~QueryMatrixSketchImpl()
{
    delete []m_dvec;
    delete []m_last_answer;
    delete []m_exact_covariance_matrix;
    delete []m_work;
    delete []m_singular_values;
    delete []m_num_vecs;
    
    if (m_var_list)
    {
        for (int i = 0; i < m_num_dirs; ++i)
        {
            delete []m_var_list[i];
        }
        delete []m_var_list;
    }

    if (m_dir_list)
    {
        for (int i = 0; i < m_num_dirs; ++i)
        {
            delete []m_dir_list[i];
        }
        delete []m_dir_list;
    }

    for (const auto &p: m_cnt_maps)
    {
        delete []p.second;
    }
    m_cnt_maps.clear();

    delete []m_ae_V;
    delete []m_ae_VT_hat;
    delete []m_ae_work2;

    if (m_fftw_plan2)
    {
        fftw_destroy_plan(m_fftw_plan2);
    }
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

    return 0;
}

int
QueryMatrixSketchImpl::additional_setup()
{
    m_use_analytic_error = g_config->get_boolean("MS.use_analytic_error").value();
    m_exact_enabled = g_config->get_boolean("EXACT_MS.enabled").value();

    if (!m_use_analytic_error)
    {
        // When we don't use analytic error and the exact answer is enabled,
        // we use EXACT_MS as the reference answer.
        if (m_exact_enabled)
        {
            if (m_sketches.size() == 0 ||
                m_sketches[0].get()->get_short_description() != "EXACT_MS")
            {
                std::cerr << "[ERROR] exact query should come first" << std::endl;
                return 1;
            }

            // these are used by print_query_summary_with_exact_answer()
            m_exact_covariance_matrix = new double[matrix_size()];
            m_work = new double[m_n * m_n];
            m_singular_values = new double[m_n];
        }
    }
    else
    {
        // Otherwise, we need to load the ground truth file to compute
        // the analytic errors.
        std::string ground_truth_file = g_config->get("MS.ground_truth_file").value();
        
        std::ifstream fin(ground_truth_file);
        if (!fin)
        {
            std::cerr << "[ERROR] unable to open the ground truth file " << ground_truth_file << std::endl;
            return 1;
        }

#define ERROR_RETURN() \
        do \
        { \
            std::cerr << "[ERROR] malformated ground truth file" << std::endl; \
            return 1; \
        } while(0)

        int d, num_dirs, has_dir_list;
        if (!(fin >> d >> num_dirs >> has_dir_list)) ERROR_RETURN();
        
        if (d != m_n)
        {
            std::cerr << "[ERROR] MS.dimension and the dimension in the ground truth file mismatch" << std::endl;
            return 1;
        }
    
        m_num_dirs = num_dirs;
        m_has_dir_list = (has_dir_list == 1);
    
        m_num_vecs = new int[m_num_dirs];
        m_var_list = new double*[m_num_dirs];
        int max_num_vecs = 0;
        for (int i = 0; i < m_num_dirs; ++i)
        {
            int num_vecs;
            if (!(fin >> num_vecs)) ERROR_RETURN();
            m_num_vecs[i] = num_vecs;
            if (num_vecs > max_num_vecs)
            {
                max_num_vecs = num_vecs;
            }
            if (!m_has_dir_list)
            {
                if (num_vecs != m_n)
                {
                    std::cerr << "[ERROR] num_vecs must equal to MS.dimension for a ground truth without dir_list" << std::endl;
                    return 1;
                }
            }
            
            m_var_list[i] = new double[num_vecs];
            for (int j = 0; j < num_vecs; ++j)
            {
                if (!(fin >> m_var_list[i][j])) ERROR_RETURN();
            }
        }
    
        if (m_has_dir_list)
        {
            m_dir_list = new double*[m_num_dirs];

            for (int k = 0; k < m_num_dirs; ++k)
            {
                int m = m_num_vecs[k]; // # of rows
                int n = m_n; // # of columns
                double *dir_mat = new double[m * n];
                for (int i = 0; i < m; ++i)
                for (int j = 0; j < n; ++j)
                {
                    if (!(fin >> dir_mat[j * m + i])) ERROR_RETURN();
                }

                m_dir_list[k] = dir_mat;
            }
        }
        
        TIMESTAMP ts;
        while (fin >> ts)
        {
            uint64_t *cnt_map = new uint64_t[m_num_dirs];
            m_cnt_maps.emplace_back(ts, cnt_map);
            for (int i = 0; i < m_num_dirs; ++i)
            {
                if (!(fin >> cnt_map[i])) ERROR_RETURN();
            }
        }
        
        m_ae_V = new double[m_n * m_ae_K];
        // m_work will be used as the work space in the first SVD of
        // print_query_summary_with_analytic_error(). Then it will be reused in
        // each call of analytic_error_mul_ata_btb().  After that, it is used
        // for storing the first m_n right singular vectors of the last SVD on
        // m_ae_V if m_n <= m_ae_K (size m_n by m_n), or is used for storing the
        // left singular vectors if m_n > m_ae_K (size m_ae_K by m_ae_K).
        m_work = new double[std::max(
            m_n * m_n,
            m_has_dir_list ? max_num_vecs : (2 * m_n))];
        m_ae_VT_hat = new double[m_n * m_n];
        m_singular_values = new double[m_n];
        m_ae_work2 = new double[std::max(m_n, m_ae_K)];

        // import the fftw3 wisdom file if available  
        if (g_config->get_boolean("misc.fftw3.import_wisdom").value())
        {
            std::string wisdom_file = g_config->get("misc.fftw3.wisdom_filename").value();
            fftw_import_wisdom_from_filename(wisdom_file.c_str());
        }

        if (!m_has_dir_list)
        {
            // Populate a few wisdoms if not available.
            // It's ok to spend a few more seconds here than later so we
            // use FFTW_PATIENT | FFTW_PRESERVE_INPUT as the flags.
            // Note that FFTW_PRESERVE_INPUT should be default for r2r transformations
            // but we specify it anyway just in case.
            //
            // Note: keep the following in sync with what's in
            // analytic_error_mul_ata_btb().
            fftw_plan plan;
            
            plan = get_dct_plan(m_n, m_ae_V, m_ae_K, m_work,
                    FFTW_WISDOM_ONLY | FFTW_PRESERVE_INPUT);
            if (!plan)
            {
                plan = get_dct_plan(m_n, m_ae_V, m_ae_K, m_work,
                    FFTW_PATIENT | FFTW_PRESERVE_INPUT);
                if (!plan)
                {
                    std::cerr << "[ERROR] unable to create fftw plan 1" << std::endl;
                    return 1;
                }
                fftw_destroy_plan(plan);
            }

            plan = get_dct_plan(m_n, m_work + m_n, 1, m_work + m_n,
                    FFTW_WISDOM_ONLY | FFTW_PRESERVE_INPUT);
            if (!plan)
            {
                plan = get_dct_plan(m_n, m_work + m_n, 1, m_work + m_n,
                    FFTW_PATIENT | FFTW_PRESERVE_INPUT);
                if (!plan)
                {
                    std::cerr << "[ERROR] unable to create fftw plan 2" << std::endl;
                    return 1;
                }

                // this one can be reused over time
            }
            m_fftw_plan2 = plan;
        }
    }

    return 0;
}

int
QueryMatrixSketchImpl::parse_query_arg(
    TIMESTAMP ts,
    const char *str)
{
    m_out << "MS(" << ts << "):" << std::endl;
    m_current_query_ts = ts;
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
    if (m_use_analytic_error)
    {
        print_query_summary_with_analytic_error(sketch);
    }
    else if (m_exact_enabled)
    {
        print_query_summary_with_exact_answer(sketch);
    }
}

void
QueryMatrixSketchImpl::print_query_summary_with_exact_answer(
    IPersistentMatrixSketch *sketch)
{
    assert(m_exact_enabled && !m_use_analytic_error);

    if (sketch == m_sketches[0].get())
    {
        std::swap(m_last_answer, m_exact_covariance_matrix);
        print_query_summary_common(sketch, -1.0);
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

        print_query_summary_common(sketch, m_singular_values[0]);
    }
}

void
QueryMatrixSketchImpl::print_query_summary_with_analytic_error(
    IPersistentMatrixSketch *sketch)
{
    constexpr uint64_t seed = 123408101995ul;
    static std::mt19937 rgen(seed);
    static std::normal_distribution std_normal(0.0, 1.0);
    
    assert(m_ae_V);
    assert(m_work);
    assert(m_singular_values);
    assert(m_ae_VT_hat);
    
    // TODO remove this svd
    // unpack m_last_answer
    {
        int k = 0;
        for (int j = 0; j < m_n; ++j)
        {
            for (int i = 0; i < j; ++i)
            {
                m_work[i * m_n + j] =
                m_work[j * m_n + i] = m_last_answer[k];
                ++k;
            }
            m_work[j* m_n + j] = m_last_answer[k];
        }
    }

    
    std::ofstream fout(std::string("output/") +
            sketch->get_short_description() +
            std::string("_ts") +
            std::to_string(m_current_query_ts) + 
            ".txt");
    for (int i = 0; i < m_n; ++i) { 
        for (int j = 0; j < m_n; ++j) {
            if (j != 0) fout << ' ';
            fout << m_work[j * m_n + i];
        }
        fout << std::endl;
    }
    fout.close();

    // u, s, vh = LA.svd(XTX)
    (void) LAPACKE_dgesdd(
        LAPACK_COL_MAJOR,
        'O',
        m_n,
        m_n,
        m_work,
        m_n,
        m_singular_values,
        nullptr,
        1,
        m_ae_VT_hat,
        m_n);

    // The values in m_work will not be used below.
    // The space will be reused in analytic_error_mul_ata_btb().
    
    // Note: sig_hat in analytic_error() of time_serial_matrix.py
    // is sqrt of m_ae_VT_hat
    int mat_size = m_ae_K * m_n;
    for (int i = 0; i < m_ae_K; ++i)
    {
        // V[i] = np.random.normal(size=self.dim)
        for (int idx = i; idx < mat_size; idx += m_ae_K) 
        {
            m_ae_V[idx] = std_normal(rgen);
        }

        for (int j = 0; j < m_ae_P; ++j)
        {
            // Vi_norm = np.linalg.norm(V[i])
            double two_norm = cblas_dnrm2(m_n, m_ae_V + i, m_ae_K);
            if (two_norm == 0) break;
            
            assert(!std::isnan(two_norm));
            
            // V[i] /= Vi_norm
            cblas_dscal(m_n, 1 / two_norm, m_ae_V + i, m_ae_K);

            // V[i] = self.mul_ata_btb(V[i], Vt_hat, sig_hat, t)
            analytic_error_mul_ata_btb(
                m_ae_V + i,
                m_ae_VT_hat,
                m_singular_values);
        }
    }
    
    // _, _, V = LA.svd(V, full_matrices=False)
    if (m_ae_K >= m_n)
    {
        (void) LAPACKE_dgesdd(
            LAPACK_COL_MAJOR,
            'O', // jobz
            m_ae_K, //m
            m_n, // n
            m_ae_V, // A
            m_ae_K, // LDA
            m_ae_work2, // S
            nullptr, // U (not referenced)
            1, // LDU
            m_work, // VT: m_n by m_n
            m_n // LDVT
        );

        // Prepare for calling analytic_error_mul_ata_btb() by copying the
        // first right singular vector to m_ae_V with a stride of m_ae_K.
        cblas_dcopy(
            m_n,
            m_work,
            m_n,
            m_ae_V,
            m_ae_K);
    }
    else
    {
        (void) LAPACKE_dgesdd(
            LAPACK_COL_MAJOR,
            'O', // jobz
            m_ae_K, // m
            m_n, // n
            m_ae_V, // A
            m_ae_K, // LDA
            m_ae_work2, // S
            m_work, // U: m_ae_K by m_ae_K
            m_ae_K, // LDU
            nullptr, // VT (not referenced)
            1  // LDVT
        );
    }

    analytic_error_mul_ata_btb(
        m_ae_V,
        m_ae_VT_hat,
        m_singular_values);
    double err = cblas_dnrm2(m_n, m_ae_V, m_ae_K);
    print_query_summary_common(sketch, err);
}

void
QueryMatrixSketchImpl::print_query_summary_common(
    IPersistentMatrixSketch *sketch,
    double err) const
{
    double exact_fnorm_sqr = get_exact_fnorm_sqr(m_current_query_ts);

    if (sketch == m_sketches[0].get())
    {
        m_out << "\tF-norm = " << std::sqrt(exact_fnorm_sqr) << std::endl;
    }
    
    // err < 0 if we do not use analytic_error and this is the EXACT sketch
    if (err >= 0.0) {
        m_out << '\t'
            << sketch->get_short_description()
            << ": "
            << "||ATA-BTB||_2 / ||A||_F^2 = "
            << err / exact_fnorm_sqr
            << std::endl;
    }
}

void
QueryMatrixSketchImpl::analytic_error_mul_ata_btb(
    double *V, /* length == m_n, stride = m_ae_K, in and out */
    double *VT_hat, /* in */
    double *sig_hat_sqr /* in */) const
{
    assert(m_ae_work2);
    assert(m_work);

    // result = np.zeros(self.dim)
    memset(m_ae_work2, 0, sizeof(double) * m_n);

    // u, counts = np.unique(...)
    // Here, u is in [0, 1, ..., m_num_dirs - 1] and counts is the cnt_map.
    auto iter = std::upper_bound(
        m_cnt_maps.begin(),
        m_cnt_maps.end(),
        m_current_query_ts,
        [](TIMESTAMP ts, const std::pair<TIMESTAMP, uint64_t*> &p) -> bool
        {
            return ts < p.first;
        });
    const uint64_t *cnt_map = (--iter)->second;

    if (!m_has_dir_list)
    {
        // precompute dct(v, 4, norm='ortho') for once and store it
        // in m_work.
        fftw_plan plan = get_dct_plan(m_n, V, m_ae_K, m_work,
                FFTW_WISDOM_ONLY | FFTW_PRESERVE_INPUT);
        assert(plan);
        fftw_execute(plan);
        fftw_destroy_plan(plan);

        double scale_factor = std::sqrt(2 * m_n);
        for (int i = 0; i < m_n; ++i)
        {
            m_work[i] /= scale_factor;
        }

        // m_fftw_plan2 performs dct(m_work + m_n, 4, norm='ortho') and in place.
        assert(m_fftw_plan2);
    }
    
    for (int i = 0; i < m_num_dirs; ++i)
    {
        if (cnt_map[i] == 0) continue;

        if (m_has_dir_list)
        {
            // result += self.dir_list[u[i]].T @ (
            //  counts[i] * self.var_list[u[i]] * (self.dir_list[u[i]] @ v))
            
            // res1 = counts[i] * self.dir_list[u[i]] @ v
            cblas_dgemv(
                CblasColMajor,
                CblasNoTrans,
                m_num_vecs[i], // m
                m_n, // n
                cnt_map[i], // alpha
                m_dir_list[i], // A = self.dir_list[u[i]]
                m_num_vecs[i], // lda
                V, // X = v
                m_ae_K, // incX
                0, // beta
                m_work, // Y
                1 // incY
                );
            
            // res2 = self.var_list[u[i]] * res1
            for (int j = 0; j < m_num_vecs[i]; ++j)
            {
                m_work[j] *= m_var_list[i][j];
            }

            // result += self.dir_list[u[i]].T @ res2
            cblas_dgemv(
                CblasColMajor,
                CblasTrans,
                m_num_vecs[i], // m
                m_n, // n
                1.0, // alpha
                m_dir_list[i], // A = self.dir_list[u[i]]
                m_num_vecs[i], //lda
                m_work, // X = res2
                1, // incX
                1, // beta
                m_ae_work2, // Y = result
                1 // incY
                );
        }
        else
        {
            // result += dct(
            //  counts[i] * self.var_list[u[i]] * dct(v, 4, norm='ortho'),
            //  4, norm='ortho')
            
            // res1 = count[i] * self.var_list[u[i]] * dct(v, 4, norm='ortho')
            for (int j = 0; j < m_n; ++j)
            {
                m_work[j + m_n] = m_work[j] * cnt_map[i] * m_var_list[i][j];
            }
            
            // res2 = dct(res1, 4) = dct(res1, 4, norm='ortho') * math.sqrt(2 * d)
            fftw_execute(m_fftw_plan2);

            // result += (1 / math.sqrt(2 * d)) * res2
            cblas_daxpy(
                m_n, // n
                1 / std::sqrt(2 * m_n), // alpha
                m_work + m_n, // X
                1, // incX
                m_ae_work2, // Y = result
                1 // incY
                );
            
        }
    }

    // now m_work is being reused again  
    // V = m_ae_work2 - VT_hat.T @ (sig_hat ** 2 * (VT_hat @ V))
    
    // res1 = VT_hat @ V
    cblas_dgemv(
        CblasColMajor,
        CblasNoTrans,
        m_n, // m
        m_n, // n
        1, // alpha
        VT_hat, // A
        m_n, // lda
        V, // X = v
        m_ae_K, // incX
        0, // beta
        m_work, // Y
        1 // incY
        );

    // res2 = sig_hat ** 2 * res1
    for (int j = 0; j < m_n; ++j)
    {
        m_work[j] *= sig_hat_sqr[j];
    }

    // V = - VT_hat.T @ res2 + m_ae_work2
    cblas_dcopy(
        m_n,
        m_ae_work2,
        1,
        V,
        m_ae_K);
    cblas_dgemv(
        CblasColMajor,
        CblasTrans,
        m_n, // m
        m_n, // n
        -1.0, // alpha
        VT_hat, // A
        m_n, // lda
        m_work, // X = res2
        1, // incX
        1.0, // beta
        V, // Y = V
        1 // incY
        );
}

fftw_plan
QueryMatrixSketchImpl::get_dct_plan(
    int n,
    double *in,
    int istride,
    double *out,
    unsigned flags)
{
    fftw_r2r_kind kind = FFTW_REDFT11; // dct type 4
    return fftw_plan_many_r2r(
        1, // rank,
        &n,
        1, // howmany
        in,
        nullptr, // inembed
        istride, // istride
        0, // idist (not used for howmany == 1)
        out,
        nullptr, // onembed
        1, // ostride
        0, // odist (not used for howmany == 1)
        &kind,
        flags);
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
    TIMESTAMP ts,
    const char *str)
{
    if (ts != m_exact_fnorm_sqr_vec.back().first)
    {
        m_exact_fnorm_sqr_vec.emplace_back(
            ts,
            m_exact_fnorm_sqr_vec.back().second);
    }

    const char *s = str;
    for (int i = 0; i < m_n; ++i)
    {
        char *s2;
        m_dvec[i] = strtod(s, &s2);
        if (s2 == s || (m_dvec[i] == HUGE_VAL)) return 1;
        s = s2;

        m_exact_fnorm_sqr_vec.back().second += m_dvec[i] * m_dvec[i];
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

void
QueryMatrixSketchImpl::finish()
{
    if (m_use_analytic_error &&
        g_config->get_boolean("misc.fftw3.export_wisdom"))
    {
        std::string wisdom_filename = g_config->get("misc.fftw3.wisdom_filename").value();
        fftw_export_wisdom_to_filename(wisdom_filename.c_str());
    }
}

double
QueryMatrixSketchImpl::get_exact_fnorm_sqr(
    TIMESTAMP ts) const
{
    auto iter = std::upper_bound(
        m_exact_fnorm_sqr_vec.begin(),
        m_exact_fnorm_sqr_vec.end(),
        ts,
        [](TIMESTAMP ts, const std::pair<TIMESTAMP, double> &p) -> bool
        {
            return ts < p.first;
        });
    assert(iter != m_exact_fnorm_sqr_vec.begin());
    return (--iter)->second;
}

/////////////////////////////////////////////////
//  Frequency estimation query implementation  //
/////////////////////////////////////////////////

template<class ISketch>
struct FESketchQueryHelper {};

template<>
struct FESketchQueryHelper<IPersistentFrequencyEstimationSketch>
{
    static uint64_t
    estimate(
        IPersistentFrequencyEstimationSketch *sketch,
        TIMESTAMP ts_e,
        uint32_t key)
    {
        return sketch->estimate_frequency(ts_e, key);
    }
};

template<>
struct FESketchQueryHelper<IPersistentFrequencyEstimationSketchBITP>
{
    static uint64_t
    estimate(
        IPersistentFrequencyEstimationSketchBITP *sketch,
        TIMESTAMP ts_s,
        uint32_t key)
    {
        return sketch->estimate_frequency_bitp(ts_s, key);
    }
};

template<class ISketch>
class QueryFrequencyEstimationImpl:
    public QueryBase<ISketch>
{
protected:
    using QueryBase<ISketch>::m_out;
    using QueryBase<ISketch>::m_sketches;

    const char *
    get_name() const
    {
        return ISketch::query_type;
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
        m_query_keys.clear();
        std::istringstream iss(str);
    
        if (m_input_is_ip)
        {
            std::string key;
            while (iss >> key)
            {
                if (key.length() != 16) return 1;
                struct in_addr ip;
                if (!inet_aton(key.c_str(), &ip))
                {
                    return 1;
                }
                m_query_keys.push_back((uint32_t) ip.s_addr);
            }
        }
        else
        {
            uint64_t key;
            while (iss >> key)
            {
                m_query_keys.push_back(key);
            }
        }
        m_out << "FE(" << ts << "):" << std::endl;

        return 0;
    }

    void
    query(
        ISketch *sketch,
        TIMESTAMP ts)
    {
        m_last_answer.clear();
        m_last_answer.reserve(m_query_keys.size());
        for (uint64_t key: m_query_keys)
        {
            uint64_t cnt = FESketchQueryHelper<ISketch>::estimate(
                    sketch, ts, key);
            m_last_answer.push_back(cnt);
        }
    }

    void
    print_query_summary(
        ISketch *sketch)
    {
        if (m_exact_enabled && sketch == m_sketches[0].get())
        {
            m_exact_answer.swap(m_last_answer);
        }
        else if (m_exact_enabled && sketch != m_sketches[0].get())
        {
            double sum_err = 0;
            double sum_err_sqr = 0;
            uint64_t max_err = 0;
            uint64_t min_err = ~(0ul);

            for (uint64_t i = 0; i < m_query_keys.size(); ++i)
            {
                uint64_t err = 
                    (m_last_answer[i] > m_exact_answer[i]) ?
                    (m_last_answer[i] - m_exact_answer[i]) :
                    (m_exact_answer[i] - m_last_answer[i]);
                sum_err += err;
                sum_err_sqr += err * err;
                if (err > max_err) max_err = err;
                if (err < min_err) min_err = err;
            }

            double avg_err = sum_err / m_query_keys.size();
            double stddev_err = std::sqrt(sum_err_sqr / m_query_keys.size()
                    - avg_err * avg_err);
            m_out << '\t'
                << sketch->get_short_description()
                << ": avg_err = "
                << avg_err
                << ", stddev = "
                << stddev_err
                << ", min_err = "
                << min_err
                << ", max_err = "
                << max_err
                << std::endl;
        }
    }

    void
    dump_query_result(
        ISketch *sketch,
        std::ostream &out,
        TIMESTAMP ts,
        uint64_t out_limit)
    {
        out << "FE_" << ts << " = {" << std::endl;
        std::vector<uint64_t> &answer = 
            (m_exact_enabled && sketch == m_sketches[0].get())
            ? m_exact_answer : m_last_answer;
        uint64_t loop_size = m_query_keys.size();
        if (out_limit != 0) {
            loop_size = std::min(out_limit, (uint64_t) m_query_keys.size());
        }
        for (uint64_t i = 0; i < loop_size; ++i)
        {
            out << '\t';
            if (m_input_is_ip)
            {
                struct in_addr ip = { .s_addr = (in_addr_t) m_query_keys[i] };
                out << '"' << inet_ntoa(ip) << "\": ";
            }
            else
            {
                out << m_query_keys[i] << ": ";
            }
            out << answer[i] << ',' << std::endl;
        }
        out << "}" << std::endl;
        if (m_query_keys.size() > out_limit)
        {
            out << "# <"
                << m_query_keys.size() - out_limit
                << " omitted" << std::endl;
        }
    }

    int
    parse_update_arg(
        TIMESTAMP ts,
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
        ISketch *sketch,
        TIMESTAMP ts)
    {
        sketch->update(ts, m_update_value);
    }

private:
    bool                        m_input_is_ip;

    bool                        m_exact_enabled;

    uint32_t                    m_update_value;

    std::vector<uint32_t>       m_query_keys;

    std::vector<uint64_t>       m_last_answer; // last estimated cnts

    std::vector<uint64_t>       m_exact_answer; // exact cnts
};

////////////////////////////////////////
//      Query public interface        //
////////////////////////////////////////

static std::vector<std::string> supported_query_types({
    "heavy_hitter",
    "heavy_hitter_bitp",
    "matrix_sketch",
    "frequency_estimation",
    "frequency_estimation_bitp"
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
    query.finish();

    return 0;
}

using QueryHeavyHitter = Query<QueryHeavyHitterImpl<IPersistentHeavyHitterSketch>>;
using QueryHeavyHitterBITP = Query<QueryHeavyHitterImpl<IPersistentHeavyHitterSketchBITP>>;
using QueryMatrixSketch = Query<QueryMatrixSketchImpl>;
using QueryFrequencyEstimation = Query<QueryFrequencyEstimationImpl<IPersistentFrequencyEstimationSketch>>;
using QueryFrequencyEstimationBITP = Query<QueryFrequencyEstimationImpl<IPersistentFrequencyEstimationSketchBITP>>;

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
    else if (query_type == "frequency_estimation")
    {
        return run_query_impl<QueryFrequencyEstimation>();
    }
    else if (query_type == "frequency_estimation_bitp")
    {
        return run_query_impl<QueryFrequencyEstimationBITP>();
    }
    
    return 2;
}


#include "exact_query.h"
#include <algorithm>
#include <numeric>
#include <sstream>
#include <cstring>
extern "C"
{
#include <cblas.h>
}
#include "conf.h"

ExactHeavyHitters::ExactHeavyHitters():
    m_items(),
    m_initial_bucket_count(m_items.bucket_count())
{
}

ExactHeavyHitters::~ExactHeavyHitters()
{
}

void
ExactHeavyHitters::clear()
{
    m_items.clear();
    m_items.rehash(m_initial_bucket_count);
}


size_t
ExactHeavyHitters::memory_usage() const
{
    // assuming gcc
    return 
        8 + // initial bucket count
        size_of_unordered_map(m_items) +
        std::accumulate(m_items.cbegin(), m_items.cend(),
            (decltype(m_items.size())) 0,
            [](auto acc, const auto &p) -> auto {
                return acc + p.second.capacity() * sizeof(Item);
            }); // item arrays
}

std::string
ExactHeavyHitters::get_short_description() const
{
    return "EXACT_HH";
}

void
ExactHeavyHitters::update(
    TIMESTAMP ts,
    uint32_t value,
    int c)
{
    std::vector<Item> &item_vec = m_items[value]; 
    if (item_vec.empty())
    {
        item_vec.emplace_back(Item{ts, (uint64_t) c});
    }
    else if (item_vec.back().m_ts == ts) {
        item_vec.back().m_cnt += c;
    }
    else
    {
        item_vec.emplace_back(Item{ts, item_vec.back().m_cnt + c});
    }
}

std::vector<IPersistentHeavyHitterSketch::HeavyHitter>
ExactHeavyHitters::estimate_heavy_hitters(
    TIMESTAMP ts_e,
    double frac_threshold) const
{
    std::vector<std::pair<uint32_t, uint64_t>> snapshot;

    uint64_t tot_cnt = 0;
    for (const auto &p: m_items)
    {
        auto &item_vec = p.second;
        auto upper_ptr = std::upper_bound(item_vec.begin(), item_vec.end(),
            ts_e, [](TIMESTAMP ts, const Item &i) -> bool { return ts < i.m_ts; });
        if (upper_ptr != item_vec.begin())
        {
            snapshot.emplace_back(p.first, upper_ptr[-1].m_cnt);
            tot_cnt += upper_ptr[-1].m_cnt;
        }
    }
    
    double threshold = frac_threshold * tot_cnt;
    std::vector<IPersistentHeavyHitterSketch::HeavyHitter> ret;
    for (const auto &item: snapshot)
    {
        if (item.second > threshold)
        {
            ret.emplace_back(IPersistentHeavyHitterSketch::HeavyHitter{
                item.first, (float) item.second / tot_cnt});
        }
    }

    return std::move(ret);
}

std::vector<IPersistentHeavyHitterSketchBITP::HeavyHitter>
ExactHeavyHitters::estimate_heavy_hitters_bitp(
    TIMESTAMP ts_s,
    double frac_threshold) const
{
    std::vector<std::pair<uint32_t, uint64_t>> snapshot;

    uint64_t tot_cnt = 0;
    for (const auto &p: m_items)
    {
        auto &item_vec = p.second;
        auto upper_ptr = std::upper_bound(item_vec.begin(),
            item_vec.end(),
            ts_s, [](TIMESTAMP ts, const Item &i) -> bool {
                return ts < i.m_ts;
            });
        
        uint64_t cnt;
        if (upper_ptr == item_vec.begin())
        {
            cnt = item_vec.back().m_cnt; 
        }
        else
        {
            cnt = item_vec.back().m_cnt - upper_ptr[-1].m_cnt;
        }

        snapshot.emplace_back(p.first, cnt);
        tot_cnt += cnt;
    }

    double threshold = frac_threshold * tot_cnt;
    std::vector<IPersistentHeavyHitterSketchBITP::HeavyHitter> ret;
    for (const auto &item: snapshot)
    {
        if (item.second > threshold)
        {
            ret.emplace_back(IPersistentHeavyHitterSketchBITP::HeavyHitter{
                item.first, (float) item.second / tot_cnt
            });
        }
    }

    return std::move(ret);
}

uint64_t
ExactHeavyHitters::estimate_frequency(
    TIMESTAMP ts_e,
    uint32_t key) const
{
    auto iter = m_items.find(key);
    if (iter == m_items.end())
    {
        return 0;
    }

    const std::vector<Item> &items = iter->second;
    auto upper_ptr = std::upper_bound(items.begin(), items.end(),
        ts_e, [](TIMESTAMP ts, const Item &i) -> bool { return ts < i.m_ts; });
    if (upper_ptr == items.begin())
    {
        return 0;
    }
    --upper_ptr;
    return upper_ptr->m_cnt;
}

uint64_t
ExactHeavyHitters::estimate_frequency_bitp(
    TIMESTAMP ts_s,
    uint32_t key) const
{
    auto iter = m_items.find(key);
    if (iter == m_items.end())
    {
        return 0;
    }

    const std::vector<Item> &items = iter->second;
    auto upper_ptr = std::upper_bound(items.begin(), items.end(),
        ts_s, [](TIMESTAMP ts, const Item &i) -> bool { return ts < i.m_ts; });
    if (upper_ptr == items.begin())
    {
        return items.back().m_cnt;
    }
    return items.back().m_cnt - (upper_ptr - 1)->m_cnt;
}

ExactHeavyHitters*
ExactHeavyHitters::create(
    int &argi,
    int argc,
    char *argv[],
    const char **help_str)
{
    if (*help_str) help_str = nullptr;
    return new ExactHeavyHitters();
}

ExactHeavyHitters*
ExactHeavyHitters::get_test_instance()
{
    return new ExactHeavyHitters();
}

ExactHeavyHitters*
ExactHeavyHitters::create_from_config(
    int idx)
{
    return new ExactHeavyHitters();
}

// ExactMatrix Implementation
ExactMatrix::ExactMatrix(
    int n):
    m_n(n),
    m_last_ts(0),
    m_cur_matrix(nullptr),
    m_matrices()
{
    if (m_n > 0)
    {
        m_cur_matrix = new double[matrix_size()];
        std::memset(m_cur_matrix, 0, sizeof(double) * matrix_size());
    }
}

ExactMatrix::~ExactMatrix()
{
    for (auto &p: m_matrices)
    {
        delete []p.second;
    }
    delete []m_cur_matrix;
}

void 
ExactMatrix::clear()
{
    for (auto &p: m_matrices)
    {
        delete []p.second;
    }
    delete []m_cur_matrix;
    m_matrices.clear();
    
    m_last_ts = 0;
    if (m_n > 0)
    {
        m_cur_matrix = new double[matrix_size()];
        std::memset(m_cur_matrix, 0, sizeof(double) * matrix_size());
    }
}

size_t
ExactMatrix::memory_usage() const
{
    return 24 +
        sizeof(double) * matrix_size() +
        sizeof(m_matrices) + m_matrices.capacity() * sizeof(m_matrices[0]) +
        m_matrices.size() * matrix_size();
}

std::string
ExactMatrix::get_short_description() const
{
    return "EXACT_MS";
}

void
ExactMatrix::update(
    TIMESTAMP       ts,
    const double    *dvec)
{
    if (m_last_ts != 0 && ts != m_last_ts)
    {
        double *m = new double[matrix_size()];
        memcpy(m, m_cur_matrix, sizeof(double) * matrix_size());
        m_matrices.emplace_back(std::make_pair(m_last_ts, m));
    }

    cblas_dspr(
        CblasColMajor,
        CblasUpper,
        m_n,
        1.0,
        dvec,
        1,
        m_cur_matrix);
    m_last_ts = ts;
}

void
ExactMatrix::get_covariance_matrix(
    TIMESTAMP       ts_e,
    double          *A) const
{
    double *m;
    if (ts_e >= m_last_ts)
    {
        m = m_cur_matrix;
    }
    else
    {
        auto iter = std::upper_bound(
            m_matrices.begin(),
            m_matrices.end(),
            ts_e,
            [](TIMESTAMP ts_e, const std::pair<TIMESTAMP, double*> &p) -> bool
            {
                return ts_e < p.first;
            });
        if (iter == m_matrices.begin())
        {
            std::memset(A, 0, sizeof(double) * matrix_size());
            return ;
        }
        m = (iter - 1)->second;
    }

    memcpy(A, m, sizeof(double) * matrix_size());
}

ExactMatrix*
ExactMatrix::get_test_instance()
{
    return new ExactMatrix(1);
}

ExactMatrix*
ExactMatrix::create_from_config(int idx)
{
    if (!g_config->is_assigned("MS.dimension")) return nullptr;

    int n = (int) g_config->get_u32("MS.dimension").value();
    return new ExactMatrix(n);
}


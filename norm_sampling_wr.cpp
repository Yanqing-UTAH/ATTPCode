#include "norm_sampling_wr.h"
#include "conf.h"
#include "avl.h"
extern "C"
{
#include <cblas.h>
}
#include <algorithm>
#include <cstring>
#include <cassert>
#include <iostream>
#include <map>

using dsimpl::UINT8;

using dsimpl::avl_impl::is_not_bit47_extended;
using dsimpl::avl_impl::xor_bit;

NormSamplingWRSketch::List::List():
    m_length(0u),
    m_capacity(0u),
    m_items(nullptr)
{}

NormSamplingWRSketch::List::~List()
{
    delete []m_items;
}

void
NormSamplingWRSketch::List::reset()
{
    for (uint32_t i = 0; i < m_length; ++i)
    {
        if (is_not_bit47_extended<63>(m_items[i].m_dvec))
        {
            delete [](xor_bit<63>(m_items[i].m_dvec));
        }
    }
    delete []m_items;
    m_items = nullptr;
    m_length = m_capacity = 0;
}

void
NormSamplingWRSketch::List::append(
    TIMESTAMP ts,
    double *dvec,
    bool is_owner)
{
    ensure_item_capacity(m_length + 1);
    
    if (is_owner)
    {
        dvec = xor_bit<63>(dvec);
    }

    m_items[m_length].m_ts = ts;
    m_items[m_length++].m_dvec = dvec;
}

size_t
NormSamplingWRSketch::List::memory_usage() const
{
    return 24 + sizeof(Item) * m_capacity;
}

double*
NormSamplingWRSketch::List::dvec_last_of(
    TIMESTAMP ts) const
{
    Item *item = std::upper_bound(m_items, m_items + m_length, ts,
        [](TIMESTAMP ts, const Item &i) -> bool {
            return ts < i.m_ts;
        });

    if (item == m_items)
    {
        return nullptr;
    }

    item = item - 1;
    if (is_not_bit47_extended<63>(item->m_dvec))
    {
        return xor_bit<63>(item->m_dvec);     
    }
    return item->m_dvec;
}

void
NormSamplingWRSketch::List::ensure_item_capacity(
    uint32_t desired_length)
{
    if (desired_length > m_capacity)
    {
        uint32_t new_capacity = (m_capacity) ? (m_capacity << 1) : 16u;
        while (desired_length > new_capacity)
        {
            new_capacity = new_capacity << 1;
        }
        Item *new_items = new Item[new_capacity];
        if (m_capacity)
        {
            memcpy(new_items, m_items, sizeof(Item) * m_capacity);
        }
        delete[] m_items;
        m_capacity = new_capacity;
        m_items = new_items;
    }
}

NormSamplingWRSketch::NormSamplingWRSketch(
    uint32_t n,
    uint32_t sample_size,
    uint32_t seed):
    m_n(n),
    m_sample_size(sample_size),
    m_last_ts(0),
    m_n_dvec_stored(0),
    m_tot_weight(0),
    m_reservoir(new List[sample_size]),
    m_rng(seed),
    m_unif_0_1(0, 1),
    m_sqr_fnorms()
{    
}

NormSamplingWRSketch::~NormSamplingWRSketch()
{
    delete []m_reservoir;
}

void
NormSamplingWRSketch::clear()
{
    m_last_ts = 0;
    m_n_dvec_stored = 0;
    for (uint32_t i = 0; i < m_sample_size; ++i)
    {
        m_reservoir[i].reset();
    }
    m_tot_weight = 0;
    m_sqr_fnorms.clear();
}

size_t
NormSamplingWRSketch::memory_usage() const
{
    size_t res = 40 + sizeof(m_rng) + sizeof(m_unif_0_1);
    for (uint32_t i = 0; i < m_sample_size; ++i)
    {
        res += m_reservoir[i].memory_usage();
    }
    res += m_n_dvec_stored * sizeof(double) * m_n;
    res += sizeof(m_sqr_fnorms) + sizeof(decltype(*m_sqr_fnorms.begin())) *
        m_sqr_fnorms.size();
    return res;
}

std::string
NormSamplingWRSketch::get_short_description() const
{
    return std::string("NORM_SAMPLING_WR-ss") + std::to_string(m_sample_size);
}

void
NormSamplingWRSketch::update(
    TIMESTAMP ts,
    const double *dvec)
{
    double l2_sqr = cblas_ddot(m_n, dvec, 1, dvec, 1);
    m_tot_weight += l2_sqr;
    
    if (m_last_ts != ts) {
        m_sqr_fnorms.emplace(m_last_ts, m_tot_weight);
        m_last_ts = ts;
    }

    double *dvec_copy = nullptr;
    for (uint32_t i = 0; i < m_sample_size; ++i)
    {
        double r = m_unif_0_1(m_rng) * m_tot_weight;
        if (r < l2_sqr) {
            if (!dvec_copy) {
                dvec_copy = new double[m_n];
                memcpy(dvec_copy, dvec, sizeof(double) * m_n);
                m_reservoir[i].append(ts, dvec_copy, true);
                ++m_n_dvec_stored;
            }
            else
            {
                m_reservoir[i].append(ts, dvec_copy, false);
            }
        }
    }
}

void
NormSamplingWRSketch::get_covariance_matrix(
    TIMESTAMP ts_e,
    double *A) const
{
    memset(A, 0, m_n * (m_n + 1) / 2 * sizeof(double));
    
    double sample_fnorm_sqr = 0;
    for (uint32_t i = 0; i < m_sample_size ; ++i)
    {
        double *dvec = m_reservoir[i].dvec_last_of(ts_e); 
        if (!dvec) return ; // which means we haven't seen any update
        double two_norm_sqr = cblas_ddot(m_n, dvec, 1, dvec, 1);
        cblas_dspr(
            CblasColMajor,
            CblasUpper,
            m_n,
            //1.0 / two_norm_sqr,
            1.0,
            dvec,
            1,
            A);
        sample_fnorm_sqr += two_norm_sqr;
    }
    
    double tot_fnorm_sqr;
    if (ts_e >= m_last_ts)
    {
        tot_fnorm_sqr = m_tot_weight;
    }
    else
    {
        auto iter = m_sqr_fnorms.upper_bound(ts_e);
        tot_fnorm_sqr = iter->second;
    }
    
    double scale_factor = tot_fnorm_sqr / sample_fnorm_sqr;
    for (size_t i = 0; i < (size_t) (m_n * (m_n + 1) / 2); ++i)
    {
        A[i] *= scale_factor; 
    }
}

NormSamplingWRSketch*
NormSamplingWRSketch::get_test_instance()
{
    return new NormSamplingWRSketch(1, 1);
}

NormSamplingWRSketch*
NormSamplingWRSketch::create_from_config(int idx)
{
    int n;
    uint32_t sample_size, seed;
    
    n = (int) g_config->get_u32("MS.dimension").value();
    sample_size = g_config->get_u32("NORM_SAMPLING_WR.sample_size", idx).value();
    seed = g_config->get_u32("NORM_SAMPLING_WR.seed", -1).value();

    return new NormSamplingWRSketch(n, sample_size, seed);
}

int
NormSamplingWRSketch::num_configs_defined()
{
    if (g_config->is_list("NORM_SAMPLING_WR.sample_size"))
    {
        return (int) g_config->list_length("NORM_SAMPLING_WR.sample_size");
    }

    return -1;
}


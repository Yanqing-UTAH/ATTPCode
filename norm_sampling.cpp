#include "norm_sampling.h"
#include "min_heap.h"
#include "conf.h"
extern "C"
{
#include <cblas.h>
}
#include <algorithm>
#include <cstring>
#include <cassert>
#include <iostream>

using dsimpl::UINT8;

NormSamplingSketch::List::List():
    m_length(0u),
    m_capacity(0u),
    m_items(nullptr),
    m_weight(0)
{}

NormSamplingSketch::List::~List()
{
    reset();
}

void
NormSamplingSketch::List::reset()
{
    for (uint32_t i = 0; i < m_length; ++i)
    {
        delete []m_items[i].m_dvec;
    }
    delete []m_items;
    m_items = nullptr;
    m_length = m_capacity = 0;
    m_weight = 0;
}

void
NormSamplingSketch::List::append(
    TIMESTAMP ts,
    double *dvec,
    double weight)
{
    ensure_item_capacity(m_length + 1);
    
    m_items[m_length].m_ts = ts;
    m_items[m_length].m_threshold = m_weight;
    m_items[m_length++].m_dvec = dvec;
    m_weight = weight;
}

size_t
NormSamplingSketch::List::memory_usage() const
{
    return 24 + sizeof(Item) * m_capacity;
}

NormSamplingSketch::Item*
NormSamplingSketch::List::last_of(
    TIMESTAMP ts) const
{
    Item *item = std::upper_bound(m_items, m_items + m_length, ts,
        [](TIMESTAMP ts, const Item &i) -> bool {
            return ts < i.m_ts;
        });

    return (item == m_items) ? nullptr : (item - 1);
}

void
NormSamplingSketch::List::ensure_item_capacity(
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

NormSamplingSketch::NormSamplingSketch(
    uint32_t n,
    uint32_t sample_size,
    uint32_t seed):
    m_n(n),
    m_sample_size(sample_size),
    m_seen(0),
    m_n_dvec_stored(0),
    m_reservoir(new List[sample_size]),
    m_weight_min_heap(new List*[sample_size]),
    m_rng(seed),
    m_unif_m1_0(-1.0, 0)
{    
    for (uint32_t i = 0; i < m_sample_size; ++i)
    {
        m_weight_min_heap[i] = &m_reservoir[i];
    }
}

NormSamplingSketch::~NormSamplingSketch()
{
    delete []m_weight_min_heap;
    delete []m_reservoir;
}

void
NormSamplingSketch::clear()
{
    m_seen = 0;
    m_n_dvec_stored = 0;
    for (uint32_t i = 0; i < m_sample_size; ++i)
    {
        m_reservoir[i].reset();
    }
    // m_weight_min_heap will be re-built anyway and thus
    // does not need be reinit'd
}

size_t
NormSamplingSketch::memory_usage() const
{
    size_t res = 40 + sizeof(m_rng) + sizeof(m_unif_m1_0);
    for (uint32_t i = 0; i < m_sample_size; ++i)
    {
        res += m_reservoir[i].memory_usage();
    }
    res += sizeof(List*) * m_sample_size; // m_weight_min_heap
    res += m_n_dvec_stored * sizeof(double) * m_n;
    return res;
}

std::string
NormSamplingSketch::get_short_description() const
{
    return std::string("NORM_SAMPLING-ss") + std::to_string(m_sample_size);
}

void
NormSamplingSketch::update(
    TIMESTAMP ts,
    const double *dvec)
{
    double l2_sqr = cblas_ddot(m_n, dvec, 1, dvec, 1);
    double weight = l2_sqr / (-m_unif_m1_0(m_rng));

    if (m_seen < m_sample_size)
    {
        double *dvec_copy = new double[m_n];
        memcpy(dvec_copy, dvec, sizeof(double) * m_n);
        m_reservoir[m_seen++].append(ts, dvec_copy, weight);
        ++m_n_dvec_stored;

        if (m_seen == m_sample_size)
        {
            dsimpl::min_heap_make(
                m_weight_min_heap,
                (UINT8) m_sample_size,
                [](List* l) -> double { return l->get_weight(); });
        }
    }
    else
    {
        if (weight > m_weight_min_heap[0]->get_weight())
        {
            double *dvec_copy = new double[m_n];
            memcpy(dvec_copy, dvec, sizeof(double) * m_n);

            List *l = m_weight_min_heap[0];
            l->append(ts, dvec_copy, weight);
            ++m_n_dvec_stored;

            dsimpl::min_heap_push_down(
                m_weight_min_heap,
                0,
                m_sample_size,
                [](List *l) -> double { return l->get_weight(); });
        }
        ++m_seen;
    }
}

void
NormSamplingSketch::get_covariance_matrix(
    TIMESTAMP ts_e,
    double *A) const
{
    memset(A, 0, m_n * (m_n + 1) / 2 * sizeof(double));
    
    uint32_t c = 0;
    const Item **items = new const Item*[m_sample_size];
    double threshold = 0;
    for (uint32_t i = 0; i < m_sample_size ; ++i)
    {
        Item *item = m_reservoir[i].last_of(ts_e); 
        if (!item) continue;
        items[c++] = item;
        if (item->m_threshold > threshold) {
            threshold = item->m_threshold;
        }
    }
    
    //uint64_t _c = 0;
    for (uint32_t i = 0; i < c; ++i) {
        double *dvec = items[i]->m_dvec;
        double l2_sqr = cblas_ddot(m_n, dvec, 1, dvec, 1);
        double alpha = (l2_sqr < threshold) ? (threshold / l2_sqr) : 1.0;
        //if (l2_sqr < threshold) ++_c;

        cblas_dspr(
            CblasColMajor,
            CblasUpper,
            m_n,
            alpha,
            dvec,
            1,
            A);
    }
    //std::cout << "_c == " << _c << std::endl;

    delete []items;
}

NormSamplingSketch*
NormSamplingSketch::get_test_instance()
{
    return new NormSamplingSketch(1, 1);
}

NormSamplingSketch*
NormSamplingSketch::create_from_config(int idx)
{
    int n;
    uint32_t sample_size, seed;
    
    n = (int) g_config->get_u32("MS.dimension").value();
    sample_size = g_config->get_u32("NORM_SAMPLING.sample_size", idx).value();
    seed = g_config->get_u32("NORM_SAMPLING.seed", -1).value();

    return new NormSamplingSketch(n, sample_size, seed);
}

int
NormSamplingSketch::num_configs_defined()
{
    if (g_config->is_list("NORM_SAMPLING.sample_size"))
    {
        return (int) g_config->list_length("NORM_SAMPLING.sample_size");
    }

    return -1;
}


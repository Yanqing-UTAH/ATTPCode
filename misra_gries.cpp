#include "misra_gries.h"
#include <cassert>
#include <iostream>
#include <cmath>

MisraGries::MisraGries(
    double epsilon):
    m_eps(epsilon),
    m_k(std::max((uint32_t) std::ceil(1 / epsilon), 2u)),
    m_cnt(),
    m_min_cnt(~0ul),
    m_delta(0),
    m_initial_bucket_count(m_cnt.bucket_count())
{
}

MisraGries::~MisraGries()
{
}

void
MisraGries::clear()
{
    m_cnt.clear();
    m_cnt.rehash(m_initial_bucket_count);
    m_min_cnt = ~0ull;
    m_delta = 0;
}

size_t
MisraGries::memory_usage() const
{
    return 16 + 56 +
        m_cnt.bucket_count() * 8 +
        m_cnt.size() * sizeof(std::__detail::_Hash_node<
            std::pair<uint32_t, uint64_t>,
            std::__cache_default<
                std::pair<uint32_t, uint64_t>, std::hash<uint32_t>>::value>);
}

void
MisraGries::update(
    uint32_t element,
    int cnt)
{
    assert(cnt > 0);
    assert(m_min_cnt > m_delta);

    auto ele_iter = m_cnt.find(element);
    if (ele_iter != m_cnt.end())
    {
        ele_iter->second += cnt;
        return ;
    }

    if (m_cnt.size() < m_k - 1)
    {
        m_cnt[element] = cnt;
        if ((unsigned) cnt < m_min_cnt)
        {
            m_min_cnt = cnt;
        }
        return ;
    }

    if ((m_delta += cnt) >= m_min_cnt)
    {
        /* 
         * first pass may or may not be able to vacate any spot because
         * m_min_cnt is lazily maintained and thus is a lower bound of the
         * actual value. Hopefully we can avoid the second pass by either
         * vacating a spot or reduce m_delta to 0.
         */
        m_delta -= m_min_cnt;
        uint64_t new_min_cnt = ~0ul;
        auto iter = m_cnt.begin();
        while (iter != m_cnt.end())
        {
            assert(iter->second >= m_min_cnt);
            if (iter->second -= m_min_cnt)
            {
                if (iter->second < new_min_cnt)
                {
                    new_min_cnt = iter->second;
                }
                ++iter;
            }
            else
            {
                auto to_remove = iter++;
                m_cnt.erase(to_remove);
            }
        }
        m_min_cnt = new_min_cnt;
    
        // nothing left to insert
        if (m_delta == 0) return;
       
        // we found a spot for the insertion
        if (m_cnt.size() < m_k - 1)
        {
            m_cnt[element] = m_delta;
            m_min_cnt = std::min(m_delta, m_min_cnt);
            m_delta = 0;
            return ;
        }
        
        /* the second pass will have to vacate some spot in the sketch for the
         * new element
         */
        if (m_delta >= m_min_cnt)
        {
            auto iter = m_cnt.begin();
            new_min_cnt = ~0ul;
            while (iter != m_cnt.end())
            {
                if (iter->second == m_min_cnt)
                {
                    auto to_remove = iter++;
                    m_cnt.erase(to_remove);
                }
                else
                {
                    new_min_cnt = std::min(new_min_cnt, iter->second -= m_min_cnt);
                    ++iter;
                }
            }

            m_delta -= m_min_cnt;
            m_min_cnt = new_min_cnt;

            assert(m_cnt.size() < m_k - 1);
            assert(m_delta <= (unsigned) cnt);

            if (m_delta == 0) return;
            m_cnt[element] = m_delta;
            m_min_cnt = std::min(m_min_cnt, m_delta);
            m_delta = 0;
            return ;
        }
    }
}

int
MisraGries::unit_test(
    int argc,
    char *argv[])
{

#define my_assert(...) \
    do \
    { \
        if (!(__VA_ARGS__)) \
        { \
            std::cout << " Fail on line " << __LINE__ << std::endl; \
            std::cout << "Aborted" << std::endl; \
            return 1; \
        } \
    } while (0)
    std::cout << "Unit tests for Misra-Gries sketch" << std::endl;
    
    std::cout << "Test 0: initial state...";
    MisraGries *mg = new MisraGries(0.3);
    my_assert(mg->m_k == 4u);
    my_assert(mg->m_cnt.size() == 0);
    my_assert(mg->m_min_cnt == ~0ull);
    my_assert(mg->m_delta == 0);
    std::cout << " Pass" << std::endl;
    
    std::cout << "Test 1: insertions of single elements...";
    mg->update(1u);
    mg->update(2u);
    mg->update(3u);
    my_assert(mg->m_cnt.size() == 3);
    my_assert(mg->m_min_cnt == 1);
    my_assert(mg->m_delta == 0);

    mg->update(4u);
    my_assert(mg->m_cnt.size() == 0);
    my_assert(mg->m_min_cnt == ~0ul);
    my_assert(mg->m_delta == 0);

    mg->update(4u);
    my_assert(mg->m_min_cnt == 1);
    my_assert(mg->m_cnt[4u] == 1);
    my_assert(mg->m_delta == 0);
    std::cout << " Pass" << std::endl;
    
    std::cout << "Test 2: clearing clear()...";
    mg->clear();
    my_assert(mg->m_cnt.size() == 0);
    my_assert(mg->m_min_cnt == ~0ul);
    my_assert(mg->m_delta == 0);
    std::cout << " Pass" << std::endl;
    
    std::cout << "Test 3: weighted insertions..."; 
    mg->update(1u, 1);
    mg->update(2u, 1);
    mg->update(3u, 10);
    mg->update(4u, 16);
    my_assert(mg->m_min_cnt == 9);
    my_assert(mg->m_delta == 0);
    my_assert(mg->m_cnt.size() == 2);
    my_assert(mg->m_cnt[3u] == 9);
    my_assert(mg->m_cnt[4u] == 15);
    std::cout << " Pass" << std::endl;
    
    std::cout << "Test 4: lazily updated min_cnt...";
    mg->update(3u, 11);
    mg->update(5u, 30);
    my_assert(mg->m_min_cnt == 9);
    my_assert(mg->m_delta == 0);
    my_assert(mg->m_cnt.size() == 3);
    my_assert(mg->m_cnt[3u] == 20);
    my_assert(mg->m_cnt[4u] == 15);
    my_assert(mg->m_cnt[5u] == 30);
    std::cout << " Pass" << std::endl;

    MisraGries *mg_save_4 = new MisraGries(*mg); 

    std::cout << "Test 5: min_cnt update 1st pass with no pending insertions...";
    mg->update(2u, 5);
    mg->update(1u, 4); 
    my_assert(mg->m_min_cnt == 6);
    my_assert(mg->m_delta == 0);
    my_assert(mg->m_cnt.size() == 3);
    my_assert(mg->m_cnt[3u] == 11);
    my_assert(mg->m_cnt[4u] == 6);
    my_assert(mg->m_cnt[5u] == 21);
    std::cout << " Pass" << std::endl;
    
    std::cout << "Test 6: min_cnt update 1st pass with insufficient pending insertions...";
    *mg = *mg_save_4;
    mg->update(2u, 5);
    mg->update(1u, 6);
    my_assert(mg->m_min_cnt == 6);
    my_assert(mg->m_delta == 2);
    my_assert(mg->m_cnt.size() == 3);
    my_assert(mg->m_cnt[3u] == 11);
    my_assert(mg->m_cnt[4u] == 6);
    my_assert(mg->m_cnt[5u] == 21);
    std::cout << " Pass" << std::endl;

    MisraGries *mg_save_6 = new MisraGries(*mg);

    std::cout << "Test 7: subsequent successful eviction with smallest cnt..."; 
    mg->update(6u, 6);
    my_assert(mg->m_min_cnt == 2);
    my_assert(mg->m_delta == 0);
    my_assert(mg->m_cnt.size() == 3);
    my_assert(mg->m_cnt[3u] == 5);
    my_assert(mg->m_cnt[5u] == 15);
    my_assert(mg->m_cnt[6u] == 2);
    std::cout << " Pass" << std::endl;

    std::cout << "Test 8: subsequent successful eviction with larger cnt...";
    *mg = *mg_save_6;
    mg->update(6u, 10);
    my_assert(mg->m_min_cnt == 5);
    my_assert(mg->m_delta == 0);
    my_assert(mg->m_cnt.size() == 3);
    my_assert(mg->m_cnt[3u] == 5);
    my_assert(mg->m_cnt[5u] == 15);
    my_assert(mg->m_cnt[6u] == 6);
    std::cout << " Pass" << std::endl;

    std::cout << "Test 9: min_cnt update 2nd pass with no pending insertions...";
    *mg = *mg_save_4;
    mg->update(2u, 5);
    mg->update(1u, 10);
    my_assert(mg->m_min_cnt == 5);
    my_assert(mg->m_delta == 0);
    my_assert(mg->m_cnt.size() == 2);
    my_assert(mg->m_cnt[3u] == 5);
    my_assert(mg->m_cnt[5u] == 15);
    std::cout << " Pass" << std::endl;

    std::cout << "Test 10: min_cnt update 2nd pass w/ inserting smallest cnt...";
    *mg = *mg_save_4;
    mg->update(2u, 5);
    mg->update(1u, 13);
    my_assert(mg->m_min_cnt == 3);
    my_assert(mg->m_delta == 0);
    my_assert(mg->m_cnt.size() == 3);
    my_assert(mg->m_cnt[3u] == 5);
    my_assert(mg->m_cnt[5u] == 15);
    my_assert(mg->m_cnt[1u] == 3);
    std::cout << " Pass" << std::endl;

    std::cout << "Test 11: min_cnt update 2nd pass w/ inserting larger cnt...";
    *mg = *mg_save_4;
    mg->update(2u, 5);
    mg->update(1u, 20);
    my_assert(mg->m_min_cnt == 5);
    my_assert(mg->m_delta == 0);
    my_assert(mg->m_cnt.size() == 3);
    my_assert(mg->m_cnt[3u] == 5);
    my_assert(mg->m_cnt[5u] == 15);
    my_assert(mg->m_cnt[1u] == 10);
    std::cout << " Pass" << std::endl;
    
    std::cout << "All tests completed" << std::endl;
    return 0;
}

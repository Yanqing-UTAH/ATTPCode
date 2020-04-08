#ifndef FENWICK_TREE_H
#define FENWICK_TREE_H

#include <type_traits>
#include <cstdint>
#include <vector>

namespace fenwick_tree {

template<typename WeightType, typename IdxType,
    typename = std::enable_if_t<std::is_unsigned_v<IdxType>>>
class fenwick_tree_t {
private:
    std::vector<WeightType> m_sum;
    typedef typename std::vector<WeightType>::size_type vec_size_t;

    inline IdxType lowbit(IdxType i) {
        return i & -i;
    }

    inline IdxType remove_lowbit(IdxType i) {
        return i & (i - 1);
    }

public:
    fenwick_tree_t(IdxType initial_size = 0)
        : m_sum(initial_size) {
        
        m_sum.shrink_to_fit();
    }

    size_t memory_usage() const {
        return m_sum.capacity() * sizeof(WeightType);
    }

    void reset(IdxType size) {
        m_sum.clear();
        m_sum.resize(size);
        m_sum.shrink_to_fit();
    }

    void set_size(IdxType size) {
        m_sum.reserve(size);
        auto tot_weight = get_prefix_sum(m_sum.size());
        while (m_sum.size() < (vec_size_t) size) {
            IdxType low = remove_lowbit((IdxType) m_sum.size() + 1);
            if (low == 0) {
                m_sum.push_back(tot_weight);
            }
            else if (low < m_sum.size()) {
                auto lower_weight = get_prefix_sum(low - 1);
                m_sum.push_back(tot_weight - lower_weight);
            } else {
                m_sum.push_back((WeightType) 0);
            }
        }
        m_sum.shrink_to_fit();
    }

    void add_weight(IdxType i, int dweight) {
        if ((vec_size_t) i >= m_sum.size()) {
            set_size(i + 1);
        }
        do {
            m_sum[i] += dweight;
            i += lowbit(i + 1); 
        } while ((vec_size_t) i < m_sum.size());
    }

    WeightType get_prefix_sum(IdxType i) {
        WeightType res;
        if ((vec_size_t) i >= m_sum.size()) {
            i = m_sum.size();
            res = 0;
        } else {
            res = m_sum[i++]; 
            i = remove_lowbit(i);
        }
        while (i > 0) {
            res += m_sum[i - 1];
            i = remove_lowbit(i);
        }
        return res;
    }
};

};

template<typename WeightType, typename IdxType = std::uint32_t>
using fenwick_tree_t = fenwick_tree::fenwick_tree_t<WeightType, IdxType>;

#endif


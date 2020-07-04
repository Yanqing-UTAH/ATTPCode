#ifndef AVL_H
#define AVL_H

#include "basic_defs.h"
#include <functional>

#include <utility>
#include <type_traits>
#include <cstring>

#if defined(IN_TEST_AVL) || !defined(NDEBUG)
#include <iostream>
#include <iomanip>

namespace dsimpl {

namespace avl_impl {

struct print_hex {
    explicit print_hex(uintptr_t v): v(v) {}
    uintptr_t v;
};

static inline std::ostream &operator<<(std::ostream &out, print_hex &&p) {
    auto flags = out.flags();
    out << "0x" << std::hex << std::setfill('0') << std::setw(16) << p.v;
    out.flags(flags);
    return out;
}

} // namespace avl_impl

} // namespace dsimpl

#include <cassert>
#define RT_ASSERT(...) assert(__VA_ARGS__)
#define DARG(...) __VA_ARGS__

#else

#define RT_ASSERT(...)
#define DARG(...)
#endif

namespace dsimpl {

namespace avl_impl {

// assuming we are using x86_64 where we have 48-bit virtual addresses
// we use a non-canonical [1] address to encode flags (e.g., an insertion handle
// in iterator)
// [1] https://en.wikipedia.org/wiki/X86-64#Virtual_address_space_details
template<int bit, typename pointer>
inline bool is_bit47_extended(pointer ptr) {
    return ((reinterpret_cast<std::uintptr_t>(ptr) >> bit) & 1) == 
        ((reinterpret_cast<std::uintptr_t>(ptr) >> 47) & 1);
}

template<int bit, typename pointer>
inline bool is_not_bit47_extended(pointer ptr) {
    return ((reinterpret_cast<std::uintptr_t>(ptr) >> bit) & 1) != 
        ((reinterpret_cast<std::uintptr_t>(ptr) >> 47) & 1);
}

template<int bit, typename pointer>
inline pointer xor_bit(pointer ptr) {
    return reinterpret_cast<pointer>(reinterpret_cast<std::uintptr_t>(ptr) ^ (1ul << bit)); 
}

template<typename node_pointer>
inline node_pointer convert_left_handle(node_pointer node) {
    return xor_bit<63>(node);
}

template<typename node_pointer>
inline node_pointer convert_right_handle(node_pointer node) {
    return xor_bit<62>(node);
}

template<typename node_pointer>
inline bool is_left_handle(node_pointer node) {
    return is_not_bit47_extended<63>(node);
}

template<typename node_pointer>
inline bool is_right_handle(node_pointer node) {
    return is_not_bit47_extended<62>(node);
}

template<typename node_pointer>
inline node_pointer convert_root_handle() {
    return xor_bit<63>(xor_bit<62, node_pointer>(nullptr));
}

template<typename node_pointer>
inline node_pointer &tree_ptr_at(node_pointer p, PayloadOffset offset) {
    return *reinterpret_cast<node_pointer*>(p->m_payload + offset);
}

template<typename node_pointer>
inline WEIGHT &_weight_nochk(node_pointer p, PayloadOffset offset) {
    return *reinterpret_cast<WEIGHT*>(p->m_payload + offset);
}

template<typename node_pointer>
inline INT4 &int4_at(node_pointer p, PayloadOffset offset) {
    return *reinterpret_cast<INT4*>(p->m_payload + offset);
}

template<typename node_pointer>
static inline WEIGHT _weight(node_pointer node, PayloadOffset offset_weight) {
    return (node) ?
        _weight_nochk(node, offset_weight)
        : 0;
}

template<class NodeType, class KeyType>
struct AVLNodeDescBase {
    typedef NodeType            node_t;
    typedef node_t              *node_pointer;
    typedef const node_t        *const_node_pointer;
    typedef node_t              &node_reference;
    typedef std::decay_t<KeyType>
                                DATUM;
    typedef DATUM               *Datum;
    typedef const DATUM         *const_Datum;
};

} // namespace avl_impl

// in namespace dsimpl
template<class NodeType, class KeyType>
struct AVLNodeDescByOffset: public avl_impl::AVLNodeDescBase<NodeType, KeyType> {
    
    typedef avl_impl::AVLNodeDescBase<NodeType, KeyType> Base;
    using typename Base::node_pointer;
    using typename Base::const_node_pointer;
    using typename Base::DATUM;
    using typename Base::Datum;
    using typename Base::const_Datum;

    
    AVLNodeDescByOffset(
        PayloadOffset   offset_key,
        PayloadOffset   offset_left,
        PayloadOffset   offset_right,
        PayloadOffset   offset_parent,
        PayloadOffset   offset_hdiff,
        std::function<bool(const DATUM&, const DATUM&)>
                        less_fn = std::less<DATUM>()):
        m_offset_key(offset_key),
        m_offset_left(offset_left),
        m_offset_right(offset_right),
        m_offset_parent(offset_parent),
        m_offset_hdiff(offset_hdiff),
        m_less_fn(less_fn) {}

    PayloadOffset               m_offset_key;

    PayloadOffset               m_offset_left;

    PayloadOffset               m_offset_right;

    PayloadOffset               m_offset_parent;

    PayloadOffset               m_offset_hdiff;

    std::function<bool(const DATUM&, const DATUM&)>
                                m_less_fn; 

    /*inline Datum _key(node_pointer node) const {
        return reinterpret_cast<Datum>(node->m_payload + m_offset_key);
    } */

    inline const DATUM &_key(const_node_pointer node) const {
        return *reinterpret_cast<const_Datum>(node->m_payload + m_offset_key);
    }

    inline node_pointer &_left(node_pointer node) const {
        return avl_impl::tree_ptr_at(node, m_offset_left);
    }

    inline node_pointer &_right(node_pointer node) const {
        return avl_impl::tree_ptr_at(node, m_offset_right);
    }

    inline node_pointer &_parent(node_pointer node) const {
        return avl_impl::tree_ptr_at(node, m_offset_parent);
    }

    inline INT4 &_hdiff(node_pointer node) const {
        return avl_impl::int4_at(node, m_offset_hdiff);
    }
};

template<class NodeType, class KeyType>
struct AVLNodeDescByOffsetWithType1SumAggregates:
    public AVLNodeDescByOffset<NodeType, KeyType> {

    using typename AVLNodeDescByOffset<NodeType, KeyType>::node_pointer;
    using typename AVLNodeDescByOffset<NodeType, KeyType>::DATUM;
    using AVLNodeDescByOffset<NodeType, KeyType>::_left;
    using AVLNodeDescByOffset<NodeType, KeyType>::_right;
    
    AVLNodeDescByOffsetWithType1SumAggregates(
        PayloadOffset   offset_key,
        PayloadOffset   offset_left,
        PayloadOffset   offset_right,
        PayloadOffset   offset_parent,
        PayloadOffset   offset_hdiff,
        UINT4           n_sums,
        const PayloadOffset
                        *offset_sums,
        const PayloadOffset
                        *offset_subtree_sums):
        AVLNodeDescByOffset<NodeType, KeyType>(
            offset_key,
            offset_left,
            offset_right,
            offset_parent,
            offset_hdiff),
        m_n_sums(n_sums) {
        
        copy_offsets(offset_sums, offset_subtree_sums);
    }

    AVLNodeDescByOffsetWithType1SumAggregates(
        PayloadOffset   offset_key,
        PayloadOffset   offset_left,
        PayloadOffset   offset_right,
        PayloadOffset   offset_parent,
        PayloadOffset   offset_hdiff,
        std::function<bool(const DATUM&, const DATUM&)>
                        less_fn,
        UINT4           n_sums,
        const PayloadOffset
                        *offset_sums,
        const PayloadOffset   
                        *offset_subtree_sums):
        AVLNodeDescByOffset<NodeType, KeyType>(
            offset_key,
            offset_left,
            offset_right,
            offset_parent,
            offset_hdiff,
            less_fn),
        m_n_sums(n_sums) {
    
        copy_offsets(offset_sums, offset_subtree_sums);
    }

    AVLNodeDescByOffsetWithType1SumAggregates(
        const AVLNodeDescByOffset<NodeType, KeyType>
                        &desc,
        UINT4           n_sums,
        const PayloadOffset
                        *offset_sums,
        const PayloadOffset
                        *offset_subtree_sums):
        AVLNodeDescByOffset<NodeType, KeyType>(desc),
        m_n_sums(n_sums) {

        copy_offsets(offset_sums, offset_subtree_sums);     
    }

    AVLNodeDescByOffsetWithType1SumAggregates(
        const AVLNodeDescByOffsetWithType1SumAggregates &other):
        AVLNodeDescByOffset<NodeType, KeyType>(other),
        m_n_sums(other.m_n_sums) {
        
        copy_offsets(other.m_offset_sums, other.m_offset_subtree_sums); 
    }

    AVLNodeDescByOffsetWithType1SumAggregates(
        AVLNodeDescByOffsetWithType1SumAggregates &&other):
        AVLNodeDescByOffset<NodeType, KeyType>(other),
        m_n_sums(other.m_n_sums),
        m_offset_sums(other.m_offset_sums),
        m_offset_subtree_sums(other.m_offset_subtree_sums) {

        other.m_offset_sums = nullptr;
        other.m_offset_subtree_sums = nullptr;
    }

    ~AVLNodeDescByOffsetWithType1SumAggregates() {
        delete[] m_offset_sums;
        delete[] m_offset_subtree_sums;
    }

    AVLNodeDescByOffsetWithType1SumAggregates&
    operator=(
        const AVLNodeDescByOffsetWithType1SumAggregates &other) {
        
        delete[] m_offset_sums;
        delete[] m_offset_subtree_sums;

        ((AVLNodeDescByOffset<NodeType, KeyType>*)this)->operator=(other);
        m_n_sums = other.m_n_sums;
        copy_offsets(other.m_offset_sums, other.m_offset_subtree_sums);

        return *this;
    }

    AVLNodeDescByOffsetWithType1SumAggregates&
    operator=(
        AVLNodeDescByOffsetWithType1SumAggregates &&other) {
        
        delete[] m_offset_sums;
        delete[] m_offset_subtree_sums;

        ((AVLNodeDescByOffset<NodeType, KeyType>*)this)->operator=(other);
        m_n_sums = other.m_n_sums;
        m_offset_sums = other.m_offset_sums;
        m_offset_subtree_sums = other.m_offset_subtree_sums;

        other.m_offset_sums = nullptr;
        other.m_offset_subtree_sums = nullptr;

        return *this;
    }
     
    void _fix_agg(node_pointer node) const {
        for (UINT4 idx = 0; idx < m_n_sums; ++idx) {
            PayloadOffset offset_w = m_offset_sums[idx];
            PayloadOffset offset_W = m_offset_subtree_sums[idx];

            avl_impl::_weight_nochk(node, offset_W) =
                avl_impl::_weight(_left(node), offset_W) +
                avl_impl::_weight(_right(node), offset_W) +
                avl_impl::_weight_nochk(node, offset_w);
        }
    }

    UINT4                   m_n_sums;
    
    PayloadOffset           *m_offset_sums = nullptr;

    PayloadOffset           *m_offset_subtree_sums = nullptr;

private:
    void copy_offsets(
        const PayloadOffset
                        *offset_sums,
        const PayloadOffset
                        *offset_subtree_sums) {
    
        m_offset_sums = new PayloadOffset[m_n_sums];    
        std::memcpy(m_offset_sums, offset_sums, m_n_sums * sizeof(PayloadOffset));
        m_offset_subtree_sums = new PayloadOffset[m_n_sums];    
        std::memcpy(m_offset_subtree_sums, offset_subtree_sums, m_n_sums * sizeof(PayloadOffset));
    }
};

enum Type2AggregateOps {
    T2AGGOPS_INSERTION,     // Tree insertion will try to use this op whenever
                            // possible.
                            // The 2nd argument to _fix_agg() is the inserted node.

    T2AGGOPS_DELETION,      // Tree deletion will try to use this op whenever
                            // possible.
                            // The 2nd argument to _fix_agg() is the deleted node.

    T2AGGOPS_DUPLICATE,     // If the aggregation is a complex structure that
                            // has ownership, it's up to the implementation
                            // whether to transfer the ownership from the old
                            // node to the new node, or actually duplicate it.
                            // The AVL implementation guarantees _fix_agg will
                            // not be called with the old node as the first
                            // argument with T2AGGOPS_INSERTION or
                            // T2AGGOPS_DELETION.
                            // The 2nd argument to _fix_agg() is the node with
                            // the aggs to copy over/move over.
                            
    T2AGGOPS_RECONSTRUCT,   // Used by insertion/deletion (rotations)
                            // The 2nd argument to _fix_agg() is nullptr

    T2AGGOPS_APPLY_DELTA    // Only used by fix_xxx_aggs public interfaces
                            // The 2nd argument to _fix_agg() is the delta_info
                            // ptr passed to fix_xxx_aggs().
};

template<class NodeType, class KeyType>
struct AVLNodeDescByOffsetWithType2SumAggregates:
    public AVLNodeDescByOffset<NodeType, KeyType> {

    using typename AVLNodeDescByOffset<NodeType, KeyType>::node_pointer;
    using typename AVLNodeDescByOffset<NodeType, KeyType>::DATUM;
    using AVLNodeDescByOffset<NodeType, KeyType>::_left;
    using AVLNodeDescByOffset<NodeType, KeyType>::_right;

    AVLNodeDescByOffsetWithType2SumAggregates(
        PayloadOffset       offset_key,
        PayloadOffset       offset_left,
        PayloadOffset       offset_right,
        PayloadOffset       offset_parent,
        PayloadOffset       offset_hdiff,
        UINT4               n_sums,
        const PayloadOffset *offset_sums,
        const PayloadOffset *offset_subtree_sums):
        AVLNodeDescByOffset<NodeType, KeyType>(
            offset_key,
            offset_left,
            offset_right,
            offset_parent,
            offset_hdiff),
        m_n_sums(n_sums) {
        
        copy_offsets(offset_sums, offset_subtree_sums);
    }

    AVLNodeDescByOffsetWithType2SumAggregates(
        PayloadOffset       offset_key,
        PayloadOffset       offset_left,
        PayloadOffset       offset_right,
        PayloadOffset       offset_parent,
        PayloadOffset       offset_hdiff,
        std::function<bool(const DATUM&, const DATUM&)>
                            less_fn,
        UINT4               n_sums,
        const PayloadOffset *offset_sums,
        const PayloadOffset *offset_subtree_sums):
        AVLNodeDescByOffset<NodeType, KeyType>(
            offset_key,
            offset_left,
            offset_right,
            offset_parent,
            offset_hdiff,
            less_fn),
        m_n_sums(n_sums) {
    
        copy_offsets(offset_sums, offset_subtree_sums);
    }

    AVLNodeDescByOffsetWithType2SumAggregates(
        const AVLNodeDescByOffset<NodeType, KeyType>
                            &desc,
        UINT4               n_sums,
        const PayloadOffset *offset_sums,
        const PayloadOffset *offset_subtree_sums):
        AVLNodeDescByOffset<NodeType, KeyType>(desc),
        m_n_sums(n_sums) {

        copy_offsets(offset_sums, offset_subtree_sums);     
    }

    AVLNodeDescByOffsetWithType2SumAggregates(
        const AVLNodeDescByOffsetWithType2SumAggregates &other):
        AVLNodeDescByOffset<NodeType, KeyType>(other),
        m_n_sums(other.m_n_sums) {
        
        copy_offsets(other.m_offset_sums, other.m_offset_subtree_sums); 
    }

    AVLNodeDescByOffsetWithType2SumAggregates(
        AVLNodeDescByOffsetWithType2SumAggregates &&other):
        AVLNodeDescByOffset<NodeType, KeyType>(other),
        m_n_sums(other.m_n_sums),
        m_offset_sums(other.m_offset_sums),
        m_offset_subtree_sums(other.m_offset_subtree_sums) {

        other.m_offset_sums = nullptr;
        other.m_offset_subtree_sums = nullptr;
    }

    ~AVLNodeDescByOffsetWithType2SumAggregates() {
        delete[] m_offset_sums;
        delete[] m_offset_subtree_sums;
    }

    AVLNodeDescByOffsetWithType2SumAggregates&
    operator=(
        const AVLNodeDescByOffsetWithType2SumAggregates &other) {
        
        delete[] m_offset_sums;
        delete[] m_offset_subtree_sums;

        ((AVLNodeDescByOffset<NodeType, KeyType>*)this)->operator=(other);
        m_n_sums = other.m_n_sums;
        copy_offsets(other.m_offset_sums, other.m_offset_subtree_sums);

        return *this;
    }

    AVLNodeDescByOffsetWithType2SumAggregates&
    operator=(
        AVLNodeDescByOffsetWithType2SumAggregates &&other) {
        
        delete[] m_offset_sums;
        delete[] m_offset_subtree_sums;

        ((AVLNodeDescByOffset<NodeType, KeyType>*)this)->operator=(other);
        m_n_sums = other.m_n_sums;
        m_offset_sums = other.m_offset_sums;
        m_offset_subtree_sums = other.m_offset_subtree_sums;

        other.m_offset_sums = nullptr;
        other.m_offset_subtree_sums = nullptr;

        return *this;
    }

    UINT4                   m_n_sums;
    
    PayloadOffset           *m_offset_sums = nullptr;

    PayloadOffset           *m_offset_subtree_sums = nullptr;
                        
private:
    void copy_offsets(
        const PayloadOffset
                        *offset_sums,
        const PayloadOffset
                        *offset_subtree_sums) {
    
        m_offset_sums = new PayloadOffset[m_n_sums];    
        std::memcpy(m_offset_sums, offset_sums, m_n_sums * sizeof(PayloadOffset));
        m_offset_subtree_sums = new PayloadOffset[m_n_sums];    
        std::memcpy(m_offset_subtree_sums, offset_subtree_sums, m_n_sums * sizeof(PayloadOffset));
    }

public:
    void _fix_agg(
        node_pointer        node,
        void                *info,
        Type2AggregateOps  op) const {
        
        node_pointer        updated;
        WEIGHT_DIFF         *weight_diffs; 

        using avl_impl::_weight;
        using avl_impl::_weight_nochk;

        switch (op) {
        case T2AGGOPS_INSERTION:
            updated = (node_pointer) info;
            for (UINT4 i = 0; i < m_n_sums; ++i) {
                _weight_nochk(node, m_offset_subtree_sums[i]) += 
                    _weight_nochk(updated, m_offset_sums[i]);
            }
            break;
        case T2AGGOPS_DELETION:
            updated = (node_pointer) info;
            for (UINT4 i = 0; i < m_n_sums; ++i) {
                _weight_nochk(node, m_offset_subtree_sums[i]) -=
                    _weight_nochk(updated, m_offset_sums[i]);
            }
            break;
        case T2AGGOPS_DUPLICATE:
            updated = (node_pointer) info;
            for (UINT4 i = 0; i < m_n_sums; ++i) {
                _weight_nochk(node, m_offset_subtree_sums[i]) =
                    _weight_nochk(updated, m_offset_subtree_sums[i]);
            }
            break;

        case T2AGGOPS_RECONSTRUCT:
            for (UINT4 i = 0; i < m_n_sums; ++i) {
                PayloadOffset offset_w = m_offset_sums[i];
                PayloadOffset offset_W = m_offset_subtree_sums[i];
                
                _weight_nochk(node, offset_W) =
                    _weight(_left(node), offset_W) +
                    _weight(_right(node), offset_W) +
                    _weight_nochk(node, offset_w);
            }
            break;

        case T2AGGOPS_APPLY_DELTA:
            weight_diffs = (WEIGHT_DIFF*) info;
            for (UINT4 i = 0; i < m_n_sums; ++i) {
                _weight_nochk(node, m_offset_subtree_sums[i]) += weight_diffs[i];
            }
            break;
        }
    }

    void prepare_delta_info(
        node_pointer        node,
        void                **p_delta_info) const {
        
        WEIGHT_DIFF *info;
        if (!*p_delta_info) {
            *p_delta_info = info = new WEIGHT_DIFF[m_n_sums];
        } else {
            info = *p_delta_info;
        }

        for (UINT4 i = 0; i < m_n_sums; ++i) {
            info[i] = _weight_nochk(node, m_offset_sums[i]);
        }
    }

    void make_delta_info(
        node_pointer        node,
        void                *delta_info) {

        WEIGHT_DIFF *info = (WEIGHT_DIFF*) delta_info;

        for (UINT4 i = 0; i < m_n_sums; ++i) {
            info[i] = ((WEIGHT_DIFF) _weight_nochk(node, m_offset_sums[i])) - info[i];
        }
    }

    void clear_delta_info(
        void                **p_delta_info) {
        
        if (*p_delta_info) {
            delete[] *p_delta_info;
            *p_delta_info = nullptr;
        }
    }
};

namespace avl_impl {

template<typename AVLNodeDescType>
struct base_class_has_predefined_aggregates {

    typedef typename AVLNodeDescType::node_pointer node_pointer;

    struct no_agg { char x[1]; };
    struct agg_type1 { char x[2]; };
    struct agg_type2 { char x[3]; };

    template<class T>
    static agg_type1
    test__fix_agg(
        decltype(((T*)nullptr)->_fix_agg((node_pointer) nullptr))*)
    { return nullptr; }

    template<class T>
    static agg_type2
    test__fix_agg(
        decltype(((T*)nullptr)->_fix_agg(
            (node_pointer) nullptr, (void*) nullptr, T2AGGOPS_INSERTION))*,
        void *)
    { return nullptr; }

    template<class T>
    static no_agg
    test__fix_agg(
        ...)
    { return nullptr; }

    static constexpr bool has_type1_agg =
        sizeof(decltype(test__fix_agg<AVLNodeDescType>(nullptr))) == 2;

    static constexpr bool has_type2_agg =
        sizeof(decltype(test__fix_agg<AVLNodeDescType>(nullptr, nullptr))) == 3;;

    static constexpr bool has_any_agg = has_type1_agg || has_type2_agg;
};

// avl_t<N>: AVL Tree with N as node type
//
// 1. N is required to have m_payload[] field if enable_additional_weights == true
// 2. AVLNodeDescType should be derived from AVLNodeDescBase and should 
//    have _key, _left, _right, _parent, _hdiff, m_less_func at minimum.
//    See AVLNodeDescByOffset, AVLNodeDescByOffsetWithType1SumAggregates and
//    AVLNodeDescByOffsetWithType2SumAggregates for examples.
// 3. Nodes are not owned by the tree. Caller should take care of
//    bookkeeping for memory allocation/deallocation
//
template<typename AVLNodeDescType, bool enable_additional_weights = true>
class avl_t:
    public AVLNodeDescType {
public:
    using typename AVLNodeDescType::node_t;
    using typename AVLNodeDescType::node_pointer;
    using typename AVLNodeDescType::node_reference;
    using typename AVLNodeDescType::DATUM;
    using typename AVLNodeDescType::Datum;
    using typename AVLNodeDescType::const_Datum;

    // NOTE don't put descriptor members in private and
    // they should be accessible from outside.
    using AVLNodeDescType::_key;
    using AVLNodeDescType::_left;
    using AVLNodeDescType::_right;
    using AVLNodeDescType::_parent;
    using AVLNodeDescType::_hdiff;
    using AVLNodeDescType::m_less_fn;

private:
    static constexpr bool       m_enable_additional_weights = enable_additional_weights;
    
    static constexpr bool       m_has_predefined_aggregates =
        base_class_has_predefined_aggregates<AVLNodeDescType>::has_any_agg;

    static constexpr bool       m_has_predefined_type1_aggregates =
        base_class_has_predefined_aggregates<AVLNodeDescType>::has_type1_agg;

    static constexpr bool       m_has_predefined_type2_aggregates =
        base_class_has_predefined_aggregates<AVLNodeDescType>::has_type2_agg;

public:
    avl_t(
        const AVLNodeDescType &avl_node_desc):
        AVLNodeDescType(avl_node_desc)
    {}

    ~avl_t() {}
    
    // TODO implement me
    avl_t(const avl_t&) = delete;
    avl_t& operator=(const avl_t&) = delete;

    avl_t(avl_t&& other):
        AVLNodeDescType(other),
        m_root(other.m_root) {
        
        other.m_root = nullptr;
    }

    avl_t& operator=(avl_t&& other) {
        ((AVLNodeDescType*) this)->operator=(other);
        m_root = other.m_root;
        other.m_root = nullptr;
    }
    
    // clone_node should not initialize left/right/parent/hdiff fields.
    // The caller should ensure the _key functions are equivalent in terms of
    // ordering between *this and other.
    void clone_from(
        const avl_t &other,
        std::function<node_pointer(node_pointer)> clone_node) {

        if (!other.m_root) {
            m_root = nullptr;
            return ;
        }
    
        // nl is a list of newly cloned nodes that have not updated their left, right
        // pointers
        node_pointer nl = clone_node(other.m_root); // a list of newly cloned nodes
        _parent(nl) = nullptr; // parent tracks the cloned parent node
        _left(nl) = other.m_root; // left tracks the original node it was cloned from
        _right(nl) = nullptr; // right serves as the next pointer in the nl list
        _hdiff(nl) = other._hdiff(other.m_root);

        node_pointer nl_tail = nl;
        m_root = nl;

        while (nl) {
            node_pointer n = nl;
            node_pointer orig_n = _left(n);
            nl = _right(nl);
            if (!nl) nl_tail = nullptr;
            
            node_pointer orig_left = other._left(orig_n);
            if (orig_left) {
                node_pointer left = clone_node(orig_left);
                _parent(left) = n;
                _left(left) = orig_left;
                if (!nl_tail) {
                    nl = nl_tail = left;
                    _right(left) = nullptr;
                } else {
                    _right(nl_tail) = left;
                    _right(left) = nullptr;
                    nl_tail = left;
                }
                _hdiff(left) = other._hdiff(orig_left);

                _left(n) = left;
            } else {
                _left(n) = nullptr;
            }

            node_pointer orig_right = other._right(orig_n);
            if (orig_right) {
                node_pointer right = clone_node(orig_right);
                _parent(right) = n;
                _left(right) = orig_right;
                if (!nl_tail) {
                    nl = nl_tail = right;
                    _right(right) = nullptr;
                } else {
                    _right(nl_tail) = right;
                    _right(right) = nullptr;
                    nl_tail = right;
                }
                _hdiff(right) = other._hdiff(orig_right);

                _right(n) = right;
            } else {
                _right(n) = nullptr;
            }
        }
    }

private:
    struct iterator_t {
        iterator_t(const avl_t *avl, node_pointer node)
            : m_avl(avl), m_node(node) {}

        iterator_t(const iterator_t&) = default;

        iterator_t &operator=(const iterator_t&) = default;

        bool operator==(const iterator_t &other) const {
            return m_avl == other.m_avl && m_node == other.m_node;
        }

        bool operator!=(const iterator_t &other) const {
            return m_avl != other.m_avl || m_node != other.m_node;
        }

        iterator_t& operator++() {
            m_node = m_avl->_next(m_node);  
            return *this; 
        }

        iterator_t operator++(int) {
            iterator_t me = *this;
            ++*this; 
            return me;
        }

        iterator_t& operator--() {
            m_node = m_avl->_prev(m_node);
            return *this;
        }

        iterator_t operator--(int) {
            iterator_t me = *this;
            --*this;
            return me;
        }

        node_reference operator*() const {
            return *m_node;
        }

        node_pointer operator->() const {
            return m_node;
        }

        bool is_null() const {
            return is_left_handle(m_node) | is_right_handle(m_node);
        }

        node_pointer get_node() const {
            return m_node;
        }

    private:
        const avl_t         *m_avl;

        node_pointer        m_node;
    
        friend class avl_t;
    };

    struct reverse_iterator_t {
        reverse_iterator_t(const avl_t *avl, node_pointer node)
            : m_avl(avl), m_node(node) {}

        reverse_iterator_t(const reverse_iterator_t&) = default;

        reverse_iterator_t &operator=(const reverse_iterator_t&) = default;

        bool operator==(const reverse_iterator_t &other) const {
            return m_avl == other.m_avl && m_node == other.m_node;
        }

        bool operator!=(const reverse_iterator_t &other) const {
            return m_avl != other.m_avl || m_node != other.m_node;
        }

        reverse_iterator_t& operator++() {
            m_node = m_avl->_prev(m_node);  
            return *this; 
        }

        reverse_iterator_t operator++(int) {
            iterator_t me = *this;
            ++*this; 
            return me;
        }

        reverse_iterator_t& operator--() {
            m_node = m_avl->_next(m_node);
            return *this;
        }

        reverse_iterator_t operator--(int) {
            iterator_t me = *this;
            --*this;
            return me;
        }

        node_reference operator*() const {
            return *m_node;
        }

        node_pointer operator->() const {
            return m_node;
        }

        bool is_null() const {
            return is_left_handle(m_node) | is_right_handle(m_node);
        }

        node_pointer get_node() const {
            return m_node;
        }

    private:
        const avl_t         *m_avl;

        node_pointer        m_node;
    
        friend class avl_t;
    };

public:
    typedef iterator_t iterator;

    typedef reverse_iterator_t reverse_iterator;

    void insert(
        node_pointer node) {
    
        node_pointer handle = _find_handle_for_insert(_key(node));
        _insert_at<false>(handle, node, 0, nullptr, nullptr);
    }
    
    template<bool b = m_enable_additional_weights>
    std::enable_if_t<b, void>
    insert(
        node_pointer node,
        UINT4 n_weights,
        const PayloadOffset *offset_weight,
        const PayloadOffset *offset_subtree_weight) {

        node_pointer handle = _find_handle_for_insert(_key(node));
        if (n_weights) {
            _insert_at<true>(
                handle,
                node,
                n_weights,
                offset_weight,
                offset_subtree_weight);
        } else {
            _insert_at<false>(
                handle,
                node,
                0,
                nullptr,
                nullptr);
        }
    }

    template<bool b = m_enable_additional_weights>
    std::enable_if_t<b, WEIGHT>
    insert_and_get_sum_left(
        node_pointer node,
        UINT4 n_weights,
        const PayloadOffset *offset_weight,
        const PayloadOffset *offset_subtree_weight,
        PayloadOffset offset_subtree_weight_for_sum_left) {
        
        WEIGHT agg;
        node_pointer handle = _find_handle_for_insert_and_get_sum_left(
            _key(node),
            offset_subtree_weight_for_sum_left,
            agg);

        if (n_weights) {
            _insert_at<true>(
                handle,
                node,
                n_weights,
                offset_weight,
                offset_subtree_weight);
        } else {
            _insert_at<false>(
                handle,
                node,
                0,
                nullptr,
                nullptr);
        }
        return agg;
    }

    iterator erase(
        iterator iter) {
    
        node_pointer n = iter.get_node();
        node_pointer n2 = next(n);
        erase(n);
        return iterator(this, n2);
    }

    void erase(
        node_pointer node) {
        
        _remove(node, 0, nullptr, nullptr);
    }
    
    template<bool b = m_enable_additional_weights>
    std::enable_if_t<b, iterator>
    erase(
        iterator iter,
        UINT4 n_weights,
        const PayloadOffset *offset_weight,
        const PayloadOffset *offset_subtree_weight) {
        
        node_pointer n = iter.get_node();
        node_pointer n2 = next(n);
        erase(n, n_weights, offset_weight, offset_subtree_weight);
        return iterator(this, n2);
    }

    template<bool b = m_enable_additional_weights>
    std::enable_if_t<b, void>
    erase(
        node_pointer node,
        UINT4 n_weights,
        const PayloadOffset *offset_weight,
        const PayloadOffset *offset_subtree_weight) {

        _remove(node,
            n_weights,
            offset_weight,
            offset_subtree_weight);
    }
    
    template<class K>
    iterator find_for_update(const K &key) const {
        return iterator(this, _find_node_or_handle_for_update(key));
    }
    
    template<class K>
    iterator find(const K &key) const {
        auto iter = find_for_update(key);
        return iter.is_null() ? end() : iter;
    }

    template<class K>
    node_pointer find_node(const K &key) const {
        node_pointer n = _find_node_or_handle_for_update(key);
        if (is_left_handle(n) || is_right_handle(n)) return nullptr;
        return n;
    }

    iterator &insert_at(
        iterator &handle,
        node_pointer node) {

        _insert_at<false>(handle.m_node, node, 0, nullptr, nullptr);
        handle.m_node = node;
        return handle;
    }

    void insert_at(
        node_pointer handle,
        node_pointer node) {
        
        _insert_at<false>(handle, node, 0, nullptr, nullptr);
    }
    
    template<bool b = m_enable_additional_weights>
    std::enable_if_t<b, iterator&>
    insert_at(
        iterator &handle,
        node_pointer node,
        UINT4 n_weights,
        const PayloadOffset *offset_weights,
        const PayloadOffset *offset_subtree_weights) {
        
        if (n_weights) {
            _insert_at<true>(handle.m_node, node,
                n_weights, offset_weights, offset_subtree_weights);
        } else {
            _insert_at<false>(handle.m_node, node, 0, nullptr, nullptr);
        }
        handle.m_node = node;
        return handle;
    }

    template<bool b = m_enable_additional_weights>
    std::enable_if_t<b, void>
    insert_at(
        node_pointer handle,
        node_pointer node,
        UINT4 n_weights,
        const PayloadOffset *offset_weights,
        const PayloadOffset *offset_subtree_weights) {

        if (n_weights) {
            _insert_at<true>(handle, node,
                n_weights, offset_weights, offset_subtree_weights);
        } else {
            _insert_at<false>(handle, node, 0, nullptr, nullptr);
        }
    }

    iterator begin() const {
        if (!m_root) return iterator(this, nullptr);
        return iterator(this, _left_most(m_root));
    }

    iterator end() const {
        return iterator(this, nullptr);
    }

    node_pointer begin_node() const {
        if (!m_root) return nullptr;
        return _left_most(m_root);
    }

    reverse_iterator rbegin() const {
        if (!m_root) return reverse_iterator(this, nullptr);
        return reverse_iterator(this, _right_most(m_root));
    }

    reverse_iterator rend() const {
        return reverse_iterator(this, nullptr);
    }

    node_pointer rbegin_node() const {
        if (!m_root) return nullptr;
        return _right_most(m_root);
    }

    iterator make_iterator(node_pointer node) const {
        return iterator(this, node);
    }
    
    node_pointer next(node_pointer node) const {
        return _next(node);
    }

    node_pointer prev(node_pointer node) const {
        return _prev(node);
    }
    
    template<class K>
    iterator lower_bound(const K &key) const {
        return iterator(this, _lower_bound(key));
    }
    
    template<class K>
    node_pointer lower_bound_node(const K &key) const {
        return _lower_bound(key);
    }

    template<class K>
    iterator upper_bound(const K &key) const {
        return iterator(this, _upper_bound(key));
    }
    
    template<class K>
    node_pointer upper_bound_node(const K &key) const {
        return _upper_bound(key);
    }

    void clear() {
        m_root = nullptr;
    }

    void delete_tree(
        std::function<void(node_pointer)> deleter) {
        
        if (!m_root) return; 
    
        // nl is the list of to-be-deleted nodes
        node_pointer nl = m_root;
        _parent(nl) = nullptr; // parent serves as the next pointer of the list

        node_pointer nl_tail = nl;

        while (nl) {
            node_pointer n = nl;
            nl = _parent(nl);
            if (!nl) nl_tail = nullptr;

            if (_left(n)) {
                node_pointer l = _left(n);
                _parent(l) = nullptr;
                if (nl_tail) {
                    _parent(nl_tail) = l;
                } else {
                    nl = l;
                }
                nl_tail = l;
            }

            if (_right(n)) {
                node_pointer r = _right(n);
                _parent(r) = nullptr;
                if (nl_tail) {
                    _parent(nl_tail) = r;
                } else {
                    nl = r;
                }
                nl_tail = r;
            }

            deleter(n);
        }

        m_root = nullptr;
    }

    void swap(avl_t &other) noexcept {
        ((AVLNodeDescType*) this)->swap(other);
        swap(m_root, other.m_root);
    }

    WEIGHT sum_range_ii(
        const DATUM &lower,
        const DATUM &upper,
        PayloadOffset offset_subtree_weight) const {
        WEIGHT cnt1 = get_sum_left<false>(lower, offset_subtree_weight);
        WEIGHT cnt2 = get_sum_left<true>(upper, offset_subtree_weight);
        return (cnt2 >= cnt1) ? (cnt2 - cnt1) : 0;
    }

    WEIGHT sum_range_ie(
        const DATUM &lower,
        const DATUM &upper,
        PayloadOffset offset_subtree_weight) const {

        WEIGHT cnt1 = get_sum_left<false>(lower, offset_subtree_weight);
        WEIGHT cnt2 = get_sum_left<false>(upper, offset_subtree_weight);
        return (cnt2 >= cnt1) ? (cnt2 - cnt1) : 0;
    }

    node_pointer get_nth_node(
        WEIGHT &offset,
        PayloadOffset offset_weight,
        PayloadOffset offset_subtree_weight) const {

        node_pointer cur = m_root;

        for (; cur;) {
            // NOTE: fast path is incorrect
            // in case of many 0 valued nodes,
            // in which case, the left branch
            // could have 0 weight and the real
            // nth node is the subtree root or 
            // on the right
            //if (offset == 0) return _left_most(cur);
            WEIGHT left_sum = _weight(_left(cur), offset_subtree_weight);
            if (offset >= left_sum) {
                offset -= left_sum;
                WEIGHT cur_weight = _weight_nochk(cur, offset_weight);
                if (offset < cur_weight) return cur;
                offset -= cur_weight;
                cur = _right(cur);
            } else {
                cur = _left(cur);
            }
        }

        return cur;
    }
    
    // exclude_subtree(A& a, node_pointer n):
    //      Return true if subtracting the subtree weight from the aggregation
    //      ``a'' does not make it below ``zero'', in which case the
    //      subtraction is completed upon return. Otherwise, do nonthing and
    //      return false. The argument ``n'' is guaranteed to be non-null.
    //
    // exclude_node(A &a, node_pointer n):
    //      Return true if subtracting the node weight from the aggregation
    //      ``a'' does not make it below ``zero'', in which case the
    //      subtraction is completed upon return. Otherwise, do nothing and
    //      return false. The argument ``n'' is guaranteed to be non-null.
    template<class A, class BinOp, class BinOp2>
    node_pointer get_nth_node(
        A &offset,
        BinOp exclude_subtree,
        BinOp2 exclude_node) const {
    
        node_pointer cur = m_root;
        while (cur) {
            if (_left(cur) && !exclude_subtree(offset, _left(cur))) {
                cur = _left(cur);
            } else {
                if (!exclude_node(offset, cur)) return cur;
                cur = _right(cur);
            }
        }
        
        return nullptr;
    }

    iterator get_nth(
        WEIGHT &offset,
        PayloadOffset offset_weight,
        PayloadOffset offset_subtree_weight) const {

        return iterator(this, get_nth_node(offset, offset_weight, offset_subtree_weight));
    }

    template<class A, class BinOp, class BinOp2>
    iterator get_nth(
        A &offset,
        BinOp exclude_subtree,
        BinOp2 exclude_node) const {
    
        return get_nth_node(offset, exclude_subtree, exclude_node);
    }

    iterator get_nth_from_lower_bound(
        const DATUM &low,
        WEIGHT &offset,
        PayloadOffset offset_weight,
        PayloadOffset offset_subtree_weight) const {
        
        WEIGHT agg_left = get_sum_left<false>(low, offset_subtree_weight);
        offset += agg_left;
        return get_nth(
            offset,
            offset_weight,
            offset_subtree_weight);
    }

#if defined(IN_TEST_AVL) || !defined(NDEBUG)
    void print(
        std::function<void(std::ostream&, node_pointer)> node_printer =
            std::function<void(std::ostream&, node_pointer)>()) const {
        print(std::cout, node_printer);
    }

    void print(
        std::ostream &out,
        std::function<void(std::ostream&, node_pointer)> node_printer = 
            std::function<void(std::ostream&, node_pointer)>()) const {
        if (m_root) print(out, node_printer, m_root);     
        else out << "empty tree" << std::endl;
    }

    void print(
        std::ostream &out,
        std::function<void(std::ostream&, node_pointer)> node_printer,
        node_pointer node) const {
        out << print_hex((uintptr_t) node) << ' '
            << print_hex((uintptr_t) _left(node)) << ' '
            << print_hex((uintptr_t) _right(node)) << ' ';
        if (node_printer) {
            node_printer(out, node);
            out << ' ';
        }
        out << _hdiff(node) << ' ' << std::endl;
        if (_left(node)) print(out, node_printer, _left(node));
        if (_right(node)) print(out, node_printer, _right(node));
    }
#endif

private:
    template<class K>
    node_pointer _find_node_or_handle_for_update(const K &key) const {
        if (!m_root) return convert_root_handle<node_pointer>();
        
        node_pointer cur = m_root;
        for (;;) {
            if (m_less_fn(key, _key(cur))) {
                if (_left(cur)) {
                    cur = _left(cur);
                } else {
                    return convert_left_handle(cur);
                }
            } else if (m_less_fn(_key(cur), key)) {
                if (_right(cur)) {
                    cur = _right(cur);
                } else {
                    return convert_right_handle(cur);
                }
            } else { 
                // equal
                return cur; 
            }
        }
    
        return nullptr;
    }

    node_pointer _find_handle_for_insert(const DATUM &key) const {
        if (!m_root) return convert_root_handle<node_pointer>();

        node_pointer cur = m_root;
        for (;;) {
            if (m_less_fn(key, _key(cur))) {
                if (_left(cur)) {
                    cur = _left(cur);             
                } else {
                    return convert_left_handle(cur);
                }
            } else {
                if (_right(cur)) {
                    cur = _right(cur);
                } else {
                    return convert_right_handle(cur);
                }
            }
        }

        RT_ASSERT(false);
        return nullptr;
    }

public:
    // only look at the position immediately prior to the hint
    // (the same as in std::multiset since C++11)
    node_pointer try_finding_handle_for_insert_with_hint(
        const DATUM &key,
        node_pointer hint) {

        if (!m_root) {
            return convert_root_handle<node_pointer>();
        }

        // If hint is nullptr, it denotes the end of the tree.
        if (!hint) {
            node_pointer rm = _right_most(m_root);
            if (!m_less_fn(key, _key(rm))) {
                return convert_right_handle(rm);
            } else {
                return nullptr;
            }
        }
        
        // key must be <= _key(hint)
        if (m_less_fn(_key(hint), key)) {
            return nullptr;
        }

        if (_left(hint)) {
            node_pointer prev_node = _right_most(_left(hint));
            if (m_less_fn(key, _key(prev_node))) {
                return nullptr;
            }
            return convert_right_handle(prev_node);
        }
        
        node_pointer prev_node = hint;
        while (_parent(prev_node) && _left(_parent(prev_node)) == prev_node) {
            prev_node = _parent(prev_node);
        }
        prev_node = _parent(prev_node);
        if (prev_node && m_less_fn(key, _key(prev_node))) {
            return nullptr;
        }
        return convert_left_handle(hint);
    }

private:
    
    node_pointer _find_handle_for_insert_and_get_sum_left(
        const DATUM &key,
        PayloadOffset offset_subtree_weight,
        WEIGHT &agg) const {
        
        agg = 0;
        if (!m_root) return convert_root_handle<node_pointer>();

        node_pointer cur = m_root;
        for (;;) {
            if (m_less_fn(key, _key(cur))) {
                if (_left(cur)) {
                    cur = _left(cur);             
                } else {
                    return convert_left_handle(cur);
                }
            } else {
                if (_right(cur)) {
                    agg += _weight_nochk(cur, offset_subtree_weight)
                        - _weight_nochk(_right(cur), offset_subtree_weight);
                    cur = _right(cur);
                } else {
                    agg += _weight_nochk(cur, offset_subtree_weight);
                    return convert_right_handle(cur);
                }
            }
        }
    
        RT_ASSERT(false);
        return nullptr;
    }
    
    template<bool has_additional_aggregates>
    void _insert_at(
        node_pointer        handle,
        node_pointer        node,
        UINT4               n_weights,
        const PayloadOffset *offset_weight,
        const PayloadOffset *offset_subtree_weight) {

        node_pointer cur;
        if (is_left_handle(handle)) {
            if (!is_right_handle(handle)) {
                cur = convert_left_handle(handle);
                _left(cur) = node;
            } else {
                m_root = node;
                cur = nullptr;
            }
        } else {
            cur = convert_right_handle(handle);
            _right(cur) = node;
        }
        _parent(node) = cur;
        _left(node) = _right(node) = nullptr;
        _hdiff(node) = 0;
        
        if constexpr (has_additional_aggregates) {
            // skip checks for nullptr in _fix_agg
            for (UINT4 idx = 0; idx < n_weights; ++idx) {
                PayloadOffset offset_w = offset_weight[idx];
                PayloadOffset offset_W = offset_subtree_weight[idx];

                _weight_nochk(node, offset_W) = _weight_nochk(node, offset_w);
            }
        }
        if constexpr (m_has_predefined_type1_aggregates)
        {

            AVLNodeDescType::_fix_agg(node);
        }
        if constexpr (m_has_predefined_type2_aggregates)
        {
            AVLNodeDescType::_fix_agg(node, node, T2AGGOPS_INSERTION);
        }
        
        rebalance_for_insertion_and_fix_agg<has_additional_aggregates>(
            node,
            n_weights,
            offset_weight,
            offset_subtree_weight);
    }
    
    template<bool has_additional_aggregates>
    void rebalance_for_insertion_and_fix_agg(
        node_pointer        node, // the inserted node
        UINT4               n_weights,
        const PayloadOffset *offset_weight,
        const PayloadOffset *offset_subtree_weight) {
    
        node_pointer inserted_node = node;
        (void) inserted_node;
        node_pointer cur = _parent(node);
        auto _fix_type2_aggs = [&]() {
            if constexpr (m_has_predefined_type2_aggregates) {
                AVLNodeDescType::_fix_agg(cur, inserted_node, T2AGGOPS_INSERTION);
            }
        };
        auto _fix_all_aggs = [&]() {
            if constexpr (has_additional_aggregates) {
                _fix_agg(cur, n_weights, offset_weight, offset_subtree_weight);
            }
            if constexpr (m_has_predefined_type1_aggregates) {
                AVLNodeDescType::_fix_agg(cur);
            }
            _fix_type2_aggs();
        };

        for (; cur != nullptr; cur = _parent(node)) {
            if (node == _right(cur)) {
                if (_hdiff(cur) > 0) {
                    _fix_type2_aggs(); // type 1 and additional aggs are recomputed
                                       // during rotation
                    if (_hdiff(node) > 0) {
                        // left rotate cur
                        node = _L<has_additional_aggregates>(
                                cur,
                                n_weights,
                                offset_weight,
                                offset_subtree_weight);
                    } else {
                        // right rotate node, and then left rotate cur
                        node = _RL<has_additional_aggregates>(
                                cur,
                                n_weights,
                                offset_weight,
                                offset_subtree_weight);
                    }
                    _fix_parent(cur, node);
                    cur = _parent(node);
                    break;
                } else if (_hdiff(cur) == 0) {
                    _fix_all_aggs();
                    _hdiff(cur) = 1;
                    node = cur;
                } else {
                    _hdiff(cur) = 0;
                    break;
                }
            } else {
                // node == cur->left
                if (_hdiff(cur) < 0) {
                    _fix_type2_aggs(); // see above
                    if (_hdiff(node) > 0) {
                        // left rotate node, and then right rotate cur
                        node = _LR<has_additional_aggregates>(
                                cur,
                                n_weights,
                                offset_weight,
                                offset_subtree_weight);
                    } else {
                        // right rotate cur
                        node = _R<has_additional_aggregates>(
                                cur,
                                n_weights,
                                offset_weight,
                                offset_subtree_weight);
                    }
                    _fix_parent(cur, node);
                    cur = _parent(node);
                    break;
                } else if (_hdiff(cur) == 0) {
                    _fix_all_aggs();
                    _hdiff(cur) = -1;
                    node = cur;
                } else {
                    _hdiff(cur) = 0;
                    break;
                }
            }
        }
    
        // fix aggs up to root
        if constexpr (has_additional_aggregates || m_has_predefined_aggregates) {
            for (; cur; cur = _parent(cur)) {
                _fix_all_aggs();
            }
        }
    }
     
    void _remove(
        node_pointer node,
        UINT4 n_weights,
        const PayloadOffset *offset_weight,
        const PayloadOffset *offset_subtree_weight) {

        node_pointer rebalance_point;
        node_pointer case1_leftmost = nullptr;
        bool del_in_left;

        if (_left(node)) {
            if (_right(node)) {
                // case 1
                // both left and right are non-null
                node_pointer leftmost = _left_most(_right(node));
                
                RT_ASSERT(!_left(leftmost));
                node_pointer leftmost_parent = _parent(leftmost);
                node_pointer leftmost_right = _right(leftmost);

                _parent(leftmost) = _parent(node);
                _fix_parent(node, leftmost);
                _left(leftmost) = _left(node);
                if (_left(node)) _parent(_left(node)) = leftmost;
                _hdiff(leftmost) = _hdiff(node);

                if (leftmost_parent != node) {
                    // case 1(a):
                    // leftmost is not the right child of node
                    //
                    //          node
                    //          /  \
                    //         /    \
                    //      left    right
                    //              /   /\
                    //            ...  /__\
                    //             /
                    //          leftmost_parent
                    //            /     /\
                    //           /     /__\
                    //     leftmost
                    //            \
                    //             \
                    //         leftmost_right
                    //            /\
                    //           /__\
                    //
                    

                    // leftmost will be rebalanced and fixed with correct agg
                    // along the path to root
                    _right(leftmost) = _right(node);
                    _parent(_right(node)) = leftmost;

                    RT_ASSERT(leftmost == _left(leftmost_parent));
                    _left(leftmost_parent) = leftmost_right;
                    if (leftmost_right) _parent(leftmost_right) = leftmost_parent;

                    rebalance_point = leftmost_parent;
                    del_in_left = true;

                    if constexpr (m_has_predefined_type2_aggregates)
                    {
                        // Since leftmost replaces node, we duplicate the
                        // type-2 aggregates of node in leftmost. Then everything
                        // from leftmost_parent (inclusive) to leftmost (exclusive)
                        // needs to subtract the leftmost; and everything from
                        // leftmost (inclusive) up to root needs to subtract the old
                        // node from the aggregation.
                        AVLNodeDescType::_fix_agg(leftmost, node, T2AGGOPS_DUPLICATE);
                        case1_leftmost = leftmost;
                    }
                } else {
                    // leftmost is the right child of node
                    // case 1(b):
                    //          node (leftmost_parent)
                    //          /  \
                    //         /    \
                    //      left    leftmost
                    //                 \
                    //                  \
                    //                 leftmost_right
                    //                  /\
                    //                 /__\
                    //
                    
                    rebalance_point = leftmost;
                    del_in_left = false;

                    if constexpr (m_has_predefined_type2_aggregates)
                    {
                        // Similar to case 1(a)
                        AVLNodeDescType::_fix_agg(leftmost, node, T2AGGOPS_DUPLICATE);
                    }
                }
            } else {
                // case 2:
                // only right is null
                // the left sub-tree must be of height 1
                //
                //      node->parent
                //       |
                //       |
                //      node
                //      /
                //     /
                //    left
                
                node_pointer left = _left(node);

                RT_ASSERT(_hdiff(node) == -1);
                RT_ASSERT(_hdiff(left) == 0);
                RT_ASSERT(!_left(left));
                RT_ASSERT(!_right(left));

                // left->hdiff does not change
                // no need to fix agg on left
                _parent(left) = _parent(node);
                del_in_left = _fix_parent(node, left);
                
                rebalance_point = _parent(left);

                // Nothing to do for type 2 aggregates at this point.
                // Need to subtract node from rebalance_point (inclusive)
                // up to the root (inclusive) during rebalancing.
            }
        } else {
            if (_right(node)) {
                // case 3:
                // only left is null
                // the right sub-tree must be of height 1
                //
                //      node->parent
                //       |
                //       |
                //      node
                //         \
                //          \
                //          right
                //

                node_pointer right = _right(node);

                RT_ASSERT(_hdiff(node) == 1);
                RT_ASSERT(_hdiff(right) == 0);
                RT_ASSERT(!_left(right));
                RT_ASSERT(!_right(right));

                // right->hdiff does not change
                // no need to fix agg on right
                _parent(right) = _parent(node);
                del_in_left = _fix_parent(node, right);
                rebalance_point = _parent(right);

                // Same as case 3 for type 2 aggregates.
            } else {
                // case 4:
                // single-node sub-tree tree
                //
                //          node->parent
                //          |
                //          |
                //          node
                //

                if (!_parent(node)) {
                    m_root = nullptr;
                    rebalance_point = nullptr;
                } else {
                    if (_left(_parent(node)) == node) {
                        _left(_parent(node)) = nullptr;
                        del_in_left = true;
                    } else {
                        _right(_parent(node)) = nullptr;
                        del_in_left = false;
                    }
                    rebalance_point = _parent(node);
                    // Same as case 2 and 3 for type 2 aggregates
                }
            }
        }

        if (rebalance_point) {
            if constexpr (m_enable_additional_weights)
            {
                if (n_weights) {
                    rebalance_for_deletion_and_fix_agg<true>(
                        node,
                        rebalance_point,
                        case1_leftmost,
                        del_in_left,
                        n_weights,
                        offset_weight,
                        offset_subtree_weight);
                } else {
                    rebalance_for_deletion_and_fix_agg<false>(
                        node,
                        rebalance_point,
                        case1_leftmost,
                        del_in_left,
                        0,
                        nullptr,
                        nullptr);
                }
            }
            else
            {
                rebalance_for_deletion_and_fix_agg<false>(
                    node,
                    rebalance_point,
                    case1_leftmost,
                    del_in_left,
                    0,
                    nullptr,
                    nullptr); 
            }
        }
    }
    
    template<bool has_additional_aggregates>
    void rebalance_for_deletion_and_fix_agg(
        node_pointer deleted_node,
        node_pointer cur,
        node_pointer case1_leftmost,
        bool del_in_left,
        UINT4 n_weights,
        const PayloadOffset *offset_weight,
        const PayloadOffset *offset_subtree_weight) {
        
        bool below_case1_leftmost = case1_leftmost;
        auto _fix_type2_aggs = [&]() {
            if constexpr (m_has_predefined_type2_aggregates) {
                if (below_case1_leftmost) {
                    if (cur == case1_leftmost) {
                        below_case1_leftmost = false;
                        AVLNodeDescType::_fix_agg(cur, deleted_node, T2AGGOPS_DELETION);
                    } else {
                        AVLNodeDescType::_fix_agg(cur, case1_leftmost, T2AGGOPS_DELETION);
                    }
                } else {
                    AVLNodeDescType::_fix_agg(cur, deleted_node, T2AGGOPS_DELETION);
                }
            }
        };
        auto _fix_all_aggs = [&] {
            if constexpr (has_additional_aggregates) {
                _fix_agg(
                    cur,
                    n_weights,
                    offset_weight,
                    offset_subtree_weight);
            }
            if constexpr (m_has_predefined_type1_aggregates) {
                AVLNodeDescType::_fix_agg(cur);
            }
            _fix_type2_aggs();
        };

        while (cur) {
skip_while_condition:
            if (del_in_left) {
                if (_hdiff(cur) < 0) {
                    _fix_all_aggs();
                    _hdiff(cur) = 0;
                    if (_parent(cur)) {
                        del_in_left = _left(_parent(cur)) == cur;
                        cur = _parent(cur);
                        goto skip_while_condition;
                    } else {
                        cur = _parent(cur);
                        break;
                    }
                    //TODO REMOVE ME cur = _parent(cur);
                } else if (_hdiff(cur) == 0) {
                    // agg fixed outside the loop
                    _hdiff(cur) = 1;
                    break;
                } else { // cur->hdiff > 0
                    // now cur->diff is 2
                    _fix_type2_aggs(); // type 1 and additional aggs are recomputed
                                       // during rotation
                    if (_hdiff(_right(cur)) < 0) {
                        // height is decreased by 1
                        node_pointer node = _RL<has_additional_aggregates>(
                                cur,
                                n_weights,
                                offset_weight,
                                offset_subtree_weight);
                        del_in_left = _fix_parent(cur, node);
                        cur = _parent(node);
                    } else {
                        node_pointer node = _L<has_additional_aggregates>(
                                cur,
                                n_weights,
                                offset_weight,
                                offset_subtree_weight);
                        del_in_left = _fix_parent(cur, node);
                        cur = _parent(node);
                        if (_hdiff(node) != 0) {
                            // height does not change
                            break;
                        }
                    }
                }
            } else {
                if (_hdiff(cur) > 0) {
                    _fix_all_aggs();
                    _hdiff(cur) = 0;
                    if (_parent(cur)) {
                        del_in_left = _left(_parent(cur)) == cur;
                        cur = _parent(cur);
                        goto skip_while_condition;
                    } else {
                        cur = _parent(cur);
                        break;
                    }
                } else if (_hdiff(cur) == 0) {
                    // agg fixed outside the loop
                    _hdiff(cur) = -1;
                    break;
                } else { // cur->hdiff < 0
                    // now cur->diff is -2 
                    _fix_type2_aggs(); // see above
                    if (_hdiff(_left(cur)) > 0) {
                        // height is decrased by 1
                        node_pointer node = _LR<has_additional_aggregates>(
                                cur,
                                n_weights,
                                offset_weight,
                                offset_subtree_weight);
                        del_in_left = _fix_parent(cur, node);
                        cur = _parent(node);
                    } else {
                        node_pointer node = _R<has_additional_aggregates>(
                                cur,
                                n_weights,
                                offset_weight,
                                offset_subtree_weight);
                        del_in_left = _fix_parent(cur, node);
                        cur = _parent(node);
                        if (_hdiff(node) != 0) {
                            // height does not change
                            break;
                        }
                    }
                }
            }
        }
    
        // fix aggs up to root 
        if constexpr (has_additional_aggregates || m_has_predefined_aggregates) {
            for (; cur; cur = _parent(cur)) {
                _fix_all_aggs();
            }
        }
    }
    
    // TODO remove me
    /*template<bool has_additional_aggregates>
    inline void fix_agg_to_root(
        node_pointer        cur,
        UINT4               n_weights,
        const PayloadOffset *offset_weight,
        const PayloadOffset *offset_subtree_weight,
        node_pointer        updated_node,
        bool                is_insertion) {
        
        if constexpr (has_additional_aggregates || m_has_predefined_aggregates) {
            for (; cur; cur = _parent(cur)) {
                if constexpr (has_additional_aggregates) {
                    _fix_agg(cur, n_weights, offset_weight, offset_subtree_weight);
                }
                if constexpr (m_has_predefined_type1_aggregates) {
                    AVLNodeDescType::_fix_agg(cur);
                }
                if constexpr (m_has_predefined_type2_aggregates) {
                    // TODO fix me
                    AVLNodeDescType::_fix_agg(cur, updated_node, is_insertion);
                }
            }
        }
    } */

    inline void _fix_agg(
        node_pointer        node,
        UINT4               n_weights,
        const PayloadOffset *offset_weight,
        const PayloadOffset *offset_subtree_weight) {

        for (UINT4 idx = 0; idx < n_weights; ++idx) {
            PayloadOffset offset_w = offset_weight[idx];
            PayloadOffset offset_W = offset_subtree_weight[idx];

            _weight_nochk(node, offset_W) =
                _weight(_left(node), offset_W) + 
                _weight(_right(node), offset_W) +
                _weight_nochk(node, offset_w);
        }
    }

    inline bool _fix_parent(node_pointer old_X, node_pointer new_X) {
        if (!_parent(new_X)) {
            m_root = new_X;
            return true;
        } else {
            if (old_X == _left(_parent(new_X))) {
                _left(_parent(new_X)) = new_X; 
                return true;
            } else /*if (old_X == new_X->parent->right)*/ {
                _right(_parent(new_X)) = new_X;
                return false;
            }
        }
    }
    
    template<bool has_additional_aggregates>
    node_pointer _L(
        node_pointer        X,
        UINT4               n_weights,
        const PayloadOffset *offset_weight,
        const PayloadOffset *offset_subtree_weight) {

        node_pointer Z = _right(X);
        RT_ASSERT(Z);

        _right(X) = _left(Z);
        if (_right(X)) {
            _parent(_right(X)) = X;
        }
        _left(Z) = X;
        _parent(Z) = _parent(X);
        _parent(X) = Z;
    
        // insertion
        RT_ASSERT(_hdiff(X) == 1);
        if (_hdiff(Z) == 0) {
            // deletion only
            _hdiff(X) = 1;
            _hdiff(Z) = -1;
        } else {
            // insertion or deletion
            RT_ASSERT(_hdiff(Z) == 1);
            _hdiff(X) = 0;
            _hdiff(Z) = 0;
        }
        
        if constexpr (has_additional_aggregates) {
            _fix_agg(X, n_weights, offset_weight, offset_subtree_weight);
            _fix_agg(Z, n_weights, offset_weight, offset_subtree_weight);
        }
        if constexpr (m_has_predefined_type1_aggregates) {
            AVLNodeDescType::_fix_agg(X);
            AVLNodeDescType::_fix_agg(Z);
        }
        if constexpr (m_has_predefined_type2_aggregates) {
            AVLNodeDescType::_fix_agg(Z, X, T2AGGOPS_DUPLICATE);
            AVLNodeDescType::_fix_agg(X, nullptr, T2AGGOPS_RECONSTRUCT);
        }
        return Z;
    }

    template<bool has_additional_aggregates>
    node_pointer _RL(
        node_pointer        X,
        UINT4               n_weights,
        const PayloadOffset *offset_weight,
        const PayloadOffset *offset_subtree_weight) {

        node_pointer Z = _right(X);
        RT_ASSERT(Z);
        node_pointer W = _left(Z);
        RT_ASSERT(W);
        
        _right(X) = _left(W);
        if (_right(X)) {
            _parent(_right(X)) = X;
        }
        _left(Z) = _right(W);
        if (_left(Z)) {
            _parent(_left(Z)) = Z;
        }
        _left(W) = X;
        _right(W) = Z;
        _parent(W) = _parent(X);
        _parent(X) = _parent(Z) = W;

        RT_ASSERT(_hdiff(X) == 1);
        RT_ASSERT(_hdiff(Z) == -1);
        if (_hdiff(W) < 0) {
            _hdiff(X) = 0;
            _hdiff(Z) = 1;
        } else if (_hdiff(W) == 0) {
            _hdiff(X) = 0;
            _hdiff(Z) = 0;
        } else {
            _hdiff(X) = -1;
            _hdiff(Z) = 0;
        }

        _hdiff(W) = 0;
    
        if constexpr (has_additional_aggregates) {
            _fix_agg(X, n_weights, offset_weight, offset_subtree_weight);
            _fix_agg(Z, n_weights, offset_weight, offset_subtree_weight);
            _fix_agg(W, n_weights, offset_weight, offset_subtree_weight);
        }
        if constexpr (m_has_predefined_type1_aggregates) {
            AVLNodeDescType::_fix_agg(X);
            AVLNodeDescType::_fix_agg(Z);
            AVLNodeDescType::_fix_agg(W);
        }
        if constexpr (m_has_predefined_type2_aggregates) {
            AVLNodeDescType::_fix_agg(W, X, T2AGGOPS_DUPLICATE);
            AVLNodeDescType::_fix_agg(X, nullptr, T2AGGOPS_RECONSTRUCT);
            AVLNodeDescType::_fix_agg(Z, nullptr, T2AGGOPS_RECONSTRUCT);
        }
        return W;
    }

    template<bool has_additional_aggregates>
    node_pointer _R(
        node_pointer        X,
        UINT4               n_weights,
        const PayloadOffset *offset_weight,
        const PayloadOffset *offset_subtree_weight) {

        node_pointer Z = _left(X);
        RT_ASSERT(Z);

        _left(X) = _right(Z);
        if (_left(X)) {
            _parent(_left(X)) = X;
        }
        _right(Z) = X;
        _parent(Z) = _parent(X);
        _parent(X) = Z;

        RT_ASSERT(_hdiff(X) == -1);
        _hdiff(X) = 0;
        if (_hdiff(Z) == 0) {
            // deletion only
            _hdiff(X) = -1;
            _hdiff(Z) = 1;
        } else {
           // insertin or deletion
            RT_ASSERT(_hdiff(Z) == -1);
            _hdiff(X) = 0;
            _hdiff(Z) = 0;
        }
    
        if constexpr (has_additional_aggregates) {
            _fix_agg(X, n_weights, offset_weight, offset_subtree_weight);
            _fix_agg(Z, n_weights, offset_weight, offset_subtree_weight);
        }
        if constexpr (m_has_predefined_type1_aggregates) {
            AVLNodeDescType::_fix_agg(X);
            AVLNodeDescType::_fix_agg(Z);
        }
        if constexpr (m_has_predefined_type2_aggregates) {
            AVLNodeDescType::_fix_agg(Z, X, T2AGGOPS_DUPLICATE);
            AVLNodeDescType::_fix_agg(X, nullptr, T2AGGOPS_RECONSTRUCT);
        }
        return Z;
    }
    
    template<bool has_additional_aggregates>
    node_pointer _LR(
        node_pointer        X,
        UINT4               n_weights,
        const PayloadOffset *offset_weight,
        const PayloadOffset *offset_subtree_weight) {

        node_pointer Z = _left(X);
        RT_ASSERT(Z);
        node_pointer W = _right(Z);
        RT_ASSERT(W);

        _right(Z) = _left(W);
        if (_right(Z)) {
            _parent(_right(Z)) = Z;
        }
        _left(X) = _right(W);
        if (_left(X)) {
            _parent(_left(X)) = X;
        }
        _left(W) = Z;
        _right(W) = X;
        _parent(W) = _parent(X);
        _parent(Z) = _parent(X) = W;

        RT_ASSERT(_hdiff(X) == -1);
        RT_ASSERT(_hdiff(Z) == 1);
        if (_hdiff(W) < 0) {
            _hdiff(Z) = 0;
            _hdiff(X) = 1;
        } else if (_hdiff(W) == 0) {
            _hdiff(Z) = 0;
            _hdiff(X) = 0;
        } else {
            _hdiff(Z) = -1;
            _hdiff(X) = 0;
        }
        _hdiff(W) = 0;
        
        if constexpr (has_additional_aggregates) {
            _fix_agg(X, n_weights, offset_weight, offset_subtree_weight);
            _fix_agg(Z, n_weights, offset_weight, offset_subtree_weight);
            _fix_agg(W, n_weights, offset_weight, offset_subtree_weight);
        }
        if constexpr (m_has_predefined_type1_aggregates) {
            AVLNodeDescType::_fix_agg(X);
            AVLNodeDescType::_fix_agg(Z);
            AVLNodeDescType::_fix_agg(W);
        }
        if constexpr (m_has_predefined_type2_aggregates) {
            AVLNodeDescType::_fix_agg(W, X, T2AGGOPS_DUPLICATE);
            AVLNodeDescType::_fix_agg(Z, nullptr, T2AGGOPS_RECONSTRUCT);
            AVLNodeDescType::_fix_agg(X, nullptr, T2AGGOPS_RECONSTRUCT);
        }
        return W;
    }

    node_pointer _left_most(node_pointer node) const {
        while (_left(node)) node = _left(node);
        return node;
    }

    node_pointer _right_most(node_pointer node) const {
        while (_right(node)) node = _right(node);
        return node;
    }

    node_pointer _next(node_pointer node) const {
        if (node == nullptr) {
            if (!m_root) return nullptr;
            return _left_most(m_root);
        }
        
        if (_right(node)) {
            return _left_most(_right(node));
        }

        while (_parent(node) && _right(_parent(node)) == node) {
            node = _parent(node);
        }
        return _parent(node);
    }

    node_pointer _prev(node_pointer node) const {
        if (node == nullptr) {
            if (!m_root) return nullptr;
            return _right_most(m_root);
        }

        if (_left(node)) {
            return _right_most(_left(node));
        }

        while (_parent(node) && _left(_parent(node)) == node) {
            node = _parent(node);
        }

        return _parent(node);
    }

    template<class K>
    node_pointer _lower_bound(const K &key) const {
        node_pointer candidate = nullptr;
        node_pointer cur = m_root;
        while (cur) {
            if (m_less_fn(_key(cur), key)) {
                cur = _right(cur);
            } else /*if (cur->key >= key)*/ {
                candidate = cur;
                cur = _left(cur);
            }
        }

        return candidate;
    }

    template<class K>
    node_pointer _upper_bound(const K &key) const {
        node_pointer candidate = nullptr;
        node_pointer cur = m_root;
        while (cur) {
            if (m_less_fn(key, _key(cur))) {
                candidate = cur;
                cur = _left(cur);
            } else /* if (cur->key <= key) */ {
                cur = _right(cur);
            }
        }

        return candidate;
    }

public:
    void fix_additional_aggs(
        node_pointer        node,
        UINT4               n_weights,
        const PayloadOffset *offset_weight,
        const PayloadOffset *offset_subtree_weight) {
        
        for (; node; node = _parent(node)) {
            _fix_agg(node);
        }
    }
    
    template<bool b = m_has_predefined_type1_aggregates>
    std::enable_if_t<b, void>
    fix_predefined_type1_aggs(
        node_pointer        node) {
        
        if constexpr (m_has_predefined_type1_aggregates) {
            for (; node; node = _parent(node)) {
                AVLNodeDescType::_fix_agg(node);
            }
        }
    }
    
    template<bool b = m_has_predefined_type2_aggregates>
    std::enable_if_t<b, void>
    fix_predefined_type2_aggs(
        node_pointer        node,
        void                *delta_info) {
        
        for (; node; node = _parent(node)) {
            AVLNodeDescType::_fix_agg(node, delta_info, T2AGGOPS_APPLY_DELTA);
        }
    }
    
    template<bool b = !m_has_predefined_type2_aggregates &&
        m_has_predefined_type1_aggregates>
    std::enable_if_t<b, void>
    fix_all_aggs(
        node_pointer        node) {
        
        _fix_all_aggs<false>(node, 0, nullptr, nullptr);
    }

    template<bool b = m_has_predefined_type2_aggregates>
    std::enable_if_t<b, void>
    fix_all_aggs(
        node_pointer        node,
        void                *delta_info) {
        
        _fix_all_aggs<false>(node, delta_info, 0, nullptr, nullptr);
    }
    
    template<bool b = !m_has_predefined_type2_aggregates>
    std::enable_if_t<b, void>
    fix_all_aggs(
        node_pointer        node,
        UINT4               n_weights,
        const PayloadOffset *offset_weight,
        const PayloadOffset *offset_subtree_weight) {
        
        _fix_all_aggs<true>(node, n_weights, offset_weight, offset_subtree_weight);
    }

    template<bool b = m_has_predefined_type2_aggregates>
    std::enable_if_t<b, void>
    fix_all_aggs(
        node_pointer        node,
        void                *delta_info,
        UINT4               n_weights,
        const PayloadOffset *offset_weight,
        const PayloadOffset *offset_subtree_weight) {
        
        _fix_all_aggs<true>(
            node, delta_info, n_weights, offset_weight, offset_subtree_weight);
    }

private:
    
    template<bool has_additional_aggs, bool b = !m_has_predefined_type2_aggregates>
    std::enable_if_t<b, void> 
    _fix_all_aggs(
        node_pointer        node,
        UINT4               n_weights,
        const PayloadOffset *offset_weight,
        const PayloadOffset *offset_subtree_weight) {
        
        if constexpr (has_additional_aggs || m_has_predefined_aggregates) {
            for (; node; node = _parent(node)) {
                if constexpr (has_additional_aggs) {
                    _fix_agg(node, n_weights, offset_weight, offset_subtree_weight);
                }
                if constexpr (m_has_predefined_type1_aggregates) {
                    AVLNodeDescType::_fix_agg(node); 
                }
            }
        }
    }

    template<bool has_additional_aggs, bool b = m_has_predefined_type2_aggregates>
    std::enable_if_t<b, void>
    _fix_all_aggs(
        node_pointer        node,
        void                *delta_info,
        UINT4               n_weights,
        const PayloadOffset *offset_weight,
        const PayloadOffset *offset_subtree_weight) {
        
        for (; node; node = _parent(node)) {
            if constexpr (has_additional_aggs) {
                _fix_agg(node, n_weights, offset_weight, offset_subtree_weight);
            }
            if constexpr (m_has_predefined_type1_aggregates) {
                AVLNodeDescType::_fix_agg(node);
            }
            AVLNodeDescType::_fix_agg(node, delta_info, T2AGGOPS_APPLY_DELTA);
        }
    }

public:
    template<bool inc> 
    WEIGHT get_sum_left(
        const DATUM &key,
        PayloadOffset offset_subtree_weight) const {

	    node_pointer cur = m_root;
        WEIGHT agg = 0;
        while (cur) {
            if (inc ?
                !m_less_fn(key, _key(cur)) :
                m_less_fn(_key(cur), key)) {

                agg += _weight(cur, offset_subtree_weight) -
                    _weight(_right(cur), offset_subtree_weight);
                    
                cur = _right(cur);
            } else {
                cur = _left(cur);
            }
        }
        return agg;
    }

    template<bool inc, typename A, class BinOp, class BinOp2>
    void get_sum_left(
        const DATUM &key,
        A &agg,
        BinOp combine,
        BinOp2 exclude) const {
    
        bool combining = true;
        node_pointer cur = m_root;
        while (cur) {
            if (inc ?
                !m_less_fn(key, _key(cur)) :
                m_less_fn(_key(cur), key)) {
                
                if (combining) {
                    combine(agg, cur);
                    combining = false;
                }
                cur = _right(cur);
            } else {
                if (!combining) {
                    exclude(agg, cur);
                    combining = true;
                }
                cur = _left(cur); 
            }
        }
    }

    template<bool inc, typename A, class BinOp, class BinOp2>
    void get_sum_left_with_combine_only(
        const DATUM &key,
        A &agg,
        BinOp combine_node,
        BinOp2 combine_subtree) const {
    
        node_pointer cur = m_root;
        while (cur) {
            if (inc ?
                !m_less_fn(key, _key(cur)) :
                m_less_fn(_key(cur), key)) {
                
                combine_node(agg, cur);
                if (_left(cur)) combine_subtree(agg, _left(cur));
                cur = _right(cur);
            } else {
                cur = _left(cur);
            }
        }
    }
    
    template<bool inc>
    WEIGHT get_sum_left(
        node_pointer    node,
        PayloadOffset   offset_subtree_weight) const {
        
        WEIGHT agg;
        if constexpr (inc) {
            agg = _weight_nochk(node, offset_subtree_weight) -
                _weight(_right(node), offset_subtree_weight);
        } else {
            agg = _weight(_left(node), offset_subtree_weight);
        }

        node_pointer cur = _parent(node);
        while (cur) {
            if (node == _right(cur)) {
                agg += _weight_nochk(cur, offset_subtree_weight)
                    - _weight_nochk(node, offset_subtree_weight);
            }

            node = cur;
            cur = _parent(node);
        }

        return agg;
    }

    template<bool inc, typename A, class BinOp, class BinOp2>
    void get_sum_left(
        node_pointer    node,
        A               &agg,
        BinOp           combine,
        BinOp2          exclude) const {
    
        node_pointer cur;
        if constexpr (inc) {
            cur = node; 
        } else {
            cur = node;
            while (_parent(cur) && cur == _left(_parent(cur))) {
                cur = _parent(cur);
            }
            cur = _parent(cur);
        }
    
        while (cur) {
            node_pointer p = cur;
            while (_parent(p) && p == _right(_parent(p))) {
                p = _parent(p);
            }
            combine(agg, p);
            if (_right(cur)) exclude(agg, _right(cur));

            p = _parent(p);
            if (!p) break;
            while (_parent(p) && p == _left(_parent(p))) {
                p = _parent(p);
            }
            cur = _parent(p);
        }
        
        if constexpr (!inc) {
            if (_left(node)) combine(agg, _left(node));
        }
    }

    template<bool inc, typename A, class BinOp, class BinOp2>
    void get_sum_left_with_combine_only(
        node_pointer    node,
        A               &agg,
        BinOp           combine_node,
        BinOp2          combine_subtree) const {
        
        if constexpr (inc) {
            combine_node(agg, node);
        }
        if (_left(node)) combine_subtree(agg, _left(node)); 
        
        while (_parent(node)) {
            node_pointer cur = _parent(node);
            if (_right(cur) == node) {
                combine_node(agg, cur);
                if (_left(cur)) combine_subtree(agg, _left(cur));
            }
            node = cur;
        }
    }

    template<bool inc>
    WEIGHT get_sum_right(
        const DATUM &key,
        PayloadOffset offset_subtree_weight) const {

        node_pointer cur = m_root;
        WEIGHT agg = 0;
        while (cur) {
            if (inc ?
                !m_less_fn(_key(cur), key) :
                m_less_fn(key, _key(cur))) {
            
                agg += _weight(cur, offset_subtree_weight) -
                    _weight(_left(cur), offset_subtree_weight);
                cur = _left(cur);
            } else {
                cur = _right(cur);
            }
        }
        return agg;
    }

    template<bool inc, typename A, class BinOp, class BinOp2>
    void get_sum_right(
        const DATUM &key,
        A &agg,
        BinOp combine,
        BinOp2 exclude) const {
    
        bool combining = true;
        node_pointer cur = m_root;
        while (cur) {
            if (inc ?
                !m_less_fn(_key(cur), key) :
                m_less_fn(key, _key(cur))) {
                
                if (combining) {
                    combine(agg, cur);
                    combining = false;
                }
                cur = _left(cur);
            } else {
                if (!combining) {
                    exclude(agg, cur);
                    combining = true;
                }
                cur = _right(cur); 
            }
        }
    }

    template<bool inc, typename A, class BinOp, class BinOp2>
    void get_sum_right_with_combine_only(
        const DATUM &key,
        A &agg,
        BinOp combine_node,
        BinOp2 combine_subtree) const {
    
        node_pointer cur = m_root;
        while (cur) {
            if (inc ?
                !m_less_fn(_key(cur), key) :
                m_less_fn(key, _key(cur))) {
                
                combine_node(agg, cur);
                if (_right(cur)) combine_subtree(agg, _right(cur));
                cur = _left(cur);
            } else {
                cur = _right(cur);
            }
        }
    }

    template<bool inc>
    WEIGHT get_sum_right(
        node_pointer node,
        PayloadOffset offset_subtree_weight) const {
        
        WEIGHT agg;
        if constexpr (inc) {
            agg = _weight_nochk(node, offset_subtree_weight) -
                _weight(_left(node), offset_subtree_weight);
        } else {
            agg = _weight(_right(node), offset_subtree_weight);
        }

        node_pointer cur = _parent(node);
        while (cur) {
            if (node == _left(cur)) {
                agg += _weight_nochk(cur, offset_subtree_weight)
                    - _weight_nochk(node, offset_subtree_weight);
            }

            node = cur;
            cur = _parent(node);
        }

        return agg;
    }

    template<bool inc, typename A, class BinOp, class BinOp2>
    void get_sum_right(
        node_pointer    node,
        A               &agg,
        BinOp           combine,
        BinOp2          exclude) const {
    
        node_pointer cur;
        if constexpr (inc) {
            cur = node; 
        } else {
            cur = node;
            while (_parent(cur) && cur == _right(_parent(cur))) {
                cur = _parent(cur);
            }
            cur = _parent(cur);
        }
    
        while (cur) {
            node_pointer p = cur;
            while (_parent(p) && p == _left(_parent(p))) {
                p = _parent(p);
            }
            combine(agg, p);
            if (_left(cur)) exclude(agg, _left(cur));

            p = _parent(p);
            if (!p) break;
            while (_parent(p) && p == _right(_parent(p))) {
                p = _parent(p);
            }
            cur = _parent(p);
        }
        
        if constexpr (!inc) {
            if (_right(node)) combine(agg, _right(node));
        }
    }

    template<bool inc, typename A, class BinOp, class BinOp2>
    void get_sum_right_with_combine_only(
        node_pointer    node,
        A               &agg,
        BinOp           combine_node,
        BinOp2          combine_subtree) const {
        
        if constexpr (inc) {
            combine_node(agg, node);
        }
        if (_right(node)) combine_subtree(agg, _right(node)); 
        
        while (_parent(node)) {
            node_pointer cur = _parent(node);
            if (_left(cur) == node) {
                combine_node(agg, cur);
                if (_right(cur)) combine_subtree(agg, _right(cur));
            }
            node = cur;
        }
    }

    WEIGHT get_total_sum(PayloadOffset offset_subtree_weight) const {
        return _weight(m_root, offset_subtree_weight);
    }

    // TODO make this interface conditionally enabled if there are
    // type-2 aggregates defined; and add a new one with no delta_info
    // arguments if there are none.
    WEIGHT fix_agg_and_get_sum_left(
        node_pointer        node,
        UINT4               n_weights,
        const PayloadOffset *offset_weight,
        const PayloadOffset *offset_subtree_weight,
        PayloadOffset       offset_subtree_weight_for_sum_left,
        void                *delta_info = nullptr) {
        
        _fix_agg(node, n_weights, offset_weight, offset_subtree_weight);
        if constexpr (m_has_predefined_type1_aggregates) {
            AVLNodeDescType::_fix_agg(node);
        }
        if constexpr (m_has_predefined_type2_aggregates) {
            AVLNodeDescType::_fix_agg(node, delta_info, T2AGGOPS_APPLY_DELTA);
        }

        WEIGHT agg = _weight(_left(node), offset_subtree_weight_for_sum_left);

        node_pointer cur = _parent(node);
        while (cur) {
            for (UINT4 idx = 0; idx < n_weights; ++idx) {
                PayloadOffset offset_w = offset_weight[idx];
                PayloadOffset offset_W = offset_subtree_weight[idx];

                _weight_nochk(cur, offset_W) =
                    _weight_nochk(cur, offset_w) + 
                    _weight(_left(cur), offset_W) +
                    _weight(_right(cur), offset_W);
            }
            if constexpr (m_has_predefined_type1_aggregates) {
                AVLNodeDescType::_fix_agg(cur);
            }
            if constexpr (m_has_predefined_type2_aggregates) {
                AVLNodeDescType::_fix_agg(cur, delta_info, T2AGGOPS_APPLY_DELTA); 
            }

            if (node == _right(cur)) {
                agg += _weight_nochk(cur, offset_subtree_weight_for_sum_left)
                    - _weight_nochk(node, offset_subtree_weight_for_sum_left);
            }

            node = cur;
            cur = _parent(node);
        }
    
        return agg;
    }

private:
    node_pointer                    m_root = nullptr;

#ifndef NDEBUG
    node_pointer                    m_debug = nullptr;
#endif // NDEBUG
};

} // namespace avl_impl

using avl_impl::avl_t;

} // namespace dsimpl

#undef DARG
#undef RT_ASSERT

#endif

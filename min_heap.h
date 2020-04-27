#ifndef MIN_HEAP_H
#define MIN_HEAP_H

#include "basic_defs.h"
#include <type_traits>
#include <functional>

//
// VecT can be any type that supports subscript operator and has array-like
// behavior now.
//

namespace dsimpl {

namespace min_heap_internal {

template<class VecT>
using ValueType = std::decay_t<decltype((*((VecT*)nullptr))[0])>;

template<class VecT, class KeyFunc>
using KeyType = std::invoke_result_t<KeyFunc, ValueType<VecT>>;

template<class VecT, class KeyFunc>
using DefaultLessFunc = std::less<KeyType<VecT, KeyFunc>>;

template<class VecT, class KeyFunc>
using DefaultGreaterFunc = std::greater<KeyType<VecT, KeyFunc>>;

template<
    UINT8 BaseIdx,
    class VecT,
    class KeyFunc,
    class LessFunc>
void push_down(
    VecT                &heap,
    UINT8               i,
    UINT8               n,
    KeyFunc             key_func,
    LessFunc            less_func) {
    
    typedef ValueType<VecT> T;
    
    T item_i0 = heap[i];
    i = i - BaseIdx + 1;

    auto key_i0 = key_func(item_i0);
    UINT8 j = i << 1;
    while (j <= n) {
        auto key_j = key_func(heap[j - 1 + BaseIdx]);
        if (j < n) {
            UINT8 k = j + 1;
            auto key_k = key_func(heap[k - 1 + BaseIdx]);
            if (less_func(key_k, key_j)) {
                if (less_func(key_k, key_i0)) {
                    heap[i - 1 + BaseIdx] = heap[k - 1 + BaseIdx];
                    i = k;
                    goto continue_loop;
                } else break;
            }
        }
        if (less_func(key_j, key_i0)) {
            heap[i - 1 + BaseIdx] = heap[j - 1 + BaseIdx];
            i = j;
        } else break;
continue_loop:
        j = i << 1;
    }
    
    heap[i - 1 + BaseIdx] = item_i0;
}

template<
    UINT8 BaseIdx,
    class VecT,
    class KeyFunc,
    class LessFunc>
void make(
    VecT                &heap,
    UINT8               n,
    KeyFunc             key_func,
    LessFunc            less_func) {

    for (UINT8 i = n >> 1; i >= 1; --i) {
        push_down<BaseIdx>(
            heap,
            i - 1 + BaseIdx,
            n,
            key_func,
            less_func);
    }
}

template<
    UINT8 BaseIdx,
    class VecT,
    class KeyFunc,
    class LessFunc>
void push_up(
    VecT                &heap,
    UINT8               i,
    UINT8               n,
    KeyFunc             key_func,
    LessFunc            less_func) {
    
    typedef ValueType<VecT> T;

    T item_i0 = heap[i];
    i = i - BaseIdx + 1;

    auto key_i0 = key_func(item_i0);
    UINT8 j = i >> 1;
    while (j >= 1) {
        auto key_j = key_func(heap[j - 1 + BaseIdx]);
        if (less_func(key_i0, key_j)) {
            heap[i - 1 + BaseIdx] = heap[j - 1 + BaseIdx];
            i = j;
            j = i >> 1;
        } else break;
    }
    
    heap[i - 1 + BaseIdx] = item_i0;
}

template<
    UINT8 BaseIdx,
    class VecT,
    class KeyFunc,
    class LessFunc>
void mark_updated(
    VecT                &heap,
    UINT8               i,
    UINT8               n,
    KeyFunc             key_func,
    LessFunc            less_func) {
    
    push_up<BaseIdx>(heap, i, n, key_func, less_func);
    push_down<BaseIdx>(heap, i, n, key_func, less_func);
}

template<
    UINT8 BaseIdx,
    class VecT,
    class KeyFunc,
    class LessFunc>
void remove(
    VecT                &heap,
    UINT8               i,
    UINT8               n,
    KeyFunc             key_func,
    LessFunc            less_func)
{
    if (n == 1) return;
    if (i - BaseIdx + 1 == n) return;

    heap[i] = heap[BaseIdx + n - 1];
    mark_updated<BaseIdx>(heap, i, n - 1, key_func, less_func);
}

template<
    UINT8 BaseIdx,
    class VecT,
    class VecUINT8,
    class KeyFunc,
    class IdxFunc,
    class LessFunc,
    std::enable_if_t<std::is_same_v<UINT8, typename VecUINT8::value_type>, int> = 0>
void push_down_with_inverted_index(
    VecT                &heap,
    VecUINT8            &inverted_index,
    UINT8               i,
    UINT8               n,
    KeyFunc             key_func,
    IdxFunc             idx_func,
    LessFunc            less_func) {

    typedef ValueType<VecT> T;
    
    T item_i0 = heap[i];
    i = i - BaseIdx + 1; 

    auto key_i0 = key_func(item_i0);
    UINT8 j = i << 1;
    while (j <= n) {
        auto key_j = key_func(heap[j - 1 + BaseIdx]);
        if (j < n) {
            UINT8 k = j + 1;
            auto key_k = key_func(heap[k - 1 + BaseIdx]);
            if (less_func(key_k, key_j)) {
                if (less_func(key_k, key_i0)) {
                    heap[i - 1 + BaseIdx] = heap[k - 1 + BaseIdx];
                    inverted_index[idx_func(heap[i - 1 + BaseIdx])] = i - 1 + BaseIdx;
                    i = k;
                    goto continue_loop;
                } else break;
            }
        }
        if (less_func(key_j, key_i0)) {
            heap[i - 1 + BaseIdx] = heap[j - 1 + BaseIdx];
            inverted_index[idx_func(heap[i - 1 + BaseIdx])] = i - 1 + BaseIdx;
            i = j;
        } else break;
continue_loop:
        j = i << 1;
    }
    
    heap[i - 1 + BaseIdx] = item_i0;
    inverted_index[idx_func(item_i0)] = i - 1 + BaseIdx;
}

template<
    UINT8 BaseIdx,
    class VecT,
    class VecUINT8,
    class KeyFunc,
    class IdxFunc,
    class LessFunc,
    std::enable_if_t<std::is_same_v<UINT8, typename VecUINT8::value_type>, int> = 0>
void make_with_inverted_index(
    VecT                &heap,
    VecUINT8            &inverted_index,
    UINT8               n,
    KeyFunc             key_func,
    IdxFunc             idx_func,
    LessFunc            less_func) {

    UINT8 i = n;
    for (; i > (n >> 1); --i) {
        inverted_index[idx_func(heap[i - 1 + BaseIdx])] = i - 1 + BaseIdx; 
    }
    for (; i >= 1; --i) {
        push_down_with_inverted_index<BaseIdx>(
            heap,
            inverted_index,
            i - 1 + BaseIdx,
            n,
            key_func,
            idx_func,
            less_func);
    }
}

template<
    UINT8 BaseIdx,
    class VecT,
    class VecUINT8,
    class KeyFunc,
    class IdxFunc,
    class LessFunc,
    std::enable_if_t<std::is_same_v<UINT8, typename VecUINT8::value_type>, int> = 0>
void push_up_with_inverted_index(
    VecT                &heap,
    VecUINT8            &inverted_index,
    UINT8               i,
    UINT8               n,
    KeyFunc             key_func,
    IdxFunc             idx_func,
    LessFunc            less_func) {

    typedef ValueType<VecT> T;
    
    T item_i0 = heap[i];
    i = i - BaseIdx + 1;

    auto key_i0 = key_func(item_i0);
    UINT8 j = i >> 1;
    while (j >= 1) {
        auto key_j = key_func(heap[j - 1 + BaseIdx]);
        if (less_func(key_i0, key_j)) {
            heap[i - 1 + BaseIdx] = heap[j - 1 + BaseIdx];
            inverted_index[idx_func(heap[i - 1 + BaseIdx])] = i - 1 + BaseIdx;
            i = j;
            j = i >> 1;
        } else break;
    }
    
    heap[i - 1 + BaseIdx] = item_i0;
    inverted_index[idx_func(item_i0)] = i - 1 + BaseIdx;
}

template<
    UINT8 BaseIdx,
    class VecT,
    class VecUINT8,
    class KeyFunc,
    class IdxFunc,
    class LessFunc,
    std::enable_if_t<std::is_same_v<UINT8, typename VecUINT8::value_type>, int> = 0>
void mark_updated_with_inverted_index(
    VecT                &heap,
    VecUINT8            &inverted_index,
    UINT8               i,
    UINT8               n,
    KeyFunc             key_func,
    IdxFunc             idx_func,
    LessFunc            less_func) {
    
    push_up_with_inverted_index<BaseIdx>(
        heap, inverted_index, i, n, key_func, idx_func, less_func);
    push_down_with_inverted_index<BaseIdx>(
        heap, inverted_index, i, n, key_func, idx_func, less_func);
}

template<
    UINT8 BaseIdx,
    class VecT,
    class VecUINT8,
    class KeyFunc,
    class IdxFunc,
    class LessFunc,
    std::enable_if_t<std::is_same_v<UINT8, typename VecUINT8::value_type>, int> = 0>
void remove_with_inverted_index(
    VecT                &heap,
    VecUINT8            &inverted_index,
    UINT8               i,
    UINT8               n,
    KeyFunc             key_func,
    IdxFunc             idx_func,
    LessFunc            less_func)
{
    if (n == 1) return;
    if (i - BaseIdx + 1 == n) return;

    heap[i] = heap[BaseIdx + n - 1];
    mark_updated_with_inverted_index<BaseIdx>(
        heap, inverted_index, i, n - 1, key_func, idx_func, less_func);
}

} // min_heap_internal


// min heap interfaces
template<
    class VecT,
    class KeyFunc,
    class LessFunc = min_heap_internal::DefaultLessFunc<VecT, KeyFunc>>
void min_heap_make(
    VecT                &heap,
    UINT8               n,
    KeyFunc             key_func,
    LessFunc            less_func = LessFunc()) {
    
    min_heap_internal::make<0>(heap, n, key_func, less_func);
}

template<
    class VecT,
    class KeyFunc,
    class LessFunc = min_heap_internal::DefaultLessFunc<VecT, KeyFunc>>
void min_heap_push_down(
    VecT                &heap,
    UINT8               i,
    UINT8               n,
    KeyFunc             key_func,
    LessFunc            less_func = LessFunc()) {
    
    min_heap_internal::push_down<0>(heap, i, n, key_func, less_func);
}

template<
    class VecT,
    class KeyFunc,
    class LessFunc = min_heap_internal::DefaultLessFunc<VecT, KeyFunc>>
void min_heap_push_up(
    VecT                &heap,
    UINT8               i,
    UINT8               n,
    KeyFunc             key_func,
    LessFunc            less_func = LessFunc()) {

    min_heap_internal::push_up<0>(heap, i, n, key_func, less_func);
}

template<
    class VecT,
    class KeyFunc,
    class LessFunc = min_heap_internal::DefaultLessFunc<VecT, KeyFunc>>
void min_heap_mark_updated(
    VecT                &heap,
    UINT8               i,
    UINT8               n,
    KeyFunc             key_func,
    LessFunc            less_func = LessFunc()) {

    min_heap_internal::mark_updated<0>(heap, i, n, key_func, less_func);
}

template<
    class VecT,
    class KeyFunc,
    class LessFunc = min_heap_internal::DefaultLessFunc<VecT, KeyFunc>>
void min_heap_remove(
    VecT                &heap,
    UINT8               i,
    UINT8               n,
    KeyFunc             key_func,
    LessFunc            less_func = LessFunc()) {
    
    min_heap_internal::remove<0>(heap, i, n, key_func, less_func);
}

template<
    class VecT,
    class VecUINT8,
    class KeyFunc,
    class IdxFunc,
    class LessFunc = min_heap_internal::DefaultLessFunc<VecT, KeyFunc>>
void min_heap_with_inverted_index_make(
    VecT                &heap,
    VecUINT8            &inverted_index,
    UINT8               n,
    KeyFunc             key_func,
    IdxFunc             idx_func,
    LessFunc            less_func = LessFunc()) {
    
    min_heap_internal::make_with_inverted_index<0>(
        heap, inverted_index, n, key_func, idx_func, less_func);
}

template<
    class VecT,
    class VecUINT8,
    class KeyFunc,
    class IdxFunc,
    class LessFunc = min_heap_internal::DefaultLessFunc<VecT, KeyFunc>>
void min_heap_with_inverted_index_push_down(
    VecT                &heap,
    VecUINT8            &inverted_index,
    UINT8               i,
    UINT8               n,
    KeyFunc             key_func,
    IdxFunc             idx_func,
    LessFunc            less_func = LessFunc()) {

    min_heap_internal::push_down_with_inverted_index<0>(
        heap, inverted_index, i, n, key_func, idx_func, less_func);
}

template<
    class VecT,
    class VecUINT8,
    class KeyFunc,
    class IdxFunc,
    class LessFunc = min_heap_internal::DefaultLessFunc<VecT, KeyFunc>>
void min_heap_with_inverted_index_push_up(
    VecT                &heap,
    VecUINT8            &inverted_index,
    UINT8               i,
    UINT8               n,
    KeyFunc             key_func,
    IdxFunc             idx_func,
    LessFunc            less_func = LessFunc()) {
    
    min_heap_internal::push_up_with_inverted_index<0>(
        heap, inverted_index, i, n, key_func, idx_func, less_func);
}

template<
    class VecT,
    class VecUINT8,
    class KeyFunc,
    class IdxFunc,
    class LessFunc = min_heap_internal::DefaultLessFunc<VecT, KeyFunc>>
void min_heap_with_inverted_index_mark_updated(
    VecT                &heap,
    VecUINT8            &inverted_index,
    UINT8               i,
    UINT8               n,
    KeyFunc             key_func,
    IdxFunc             idx_func,
    LessFunc            less_func = LessFunc()) {
    
    min_heap_internal::mark_updated_with_inverted_index<0>(
        heap, inverted_index, i, n, key_func, idx_func, less_func);
}

template<
    class VecT,
    class VecUINT8,
    class KeyFunc,
    class IdxFunc,
    class LessFunc = min_heap_internal::DefaultLessFunc<VecT, KeyFunc>>
void min_heap_with_inverted_index_remove(
    VecT                &heap,
    VecUINT8            &inverted_index,
    UINT8               i,
    UINT8               n,
    KeyFunc             key_func,
    IdxFunc             idx_func,
    LessFunc            less_func = LessFunc()) {
    
    min_heap_internal::remove_with_inverted_index<0>(
        heap, inverted_index, i, n, key_func, idx_func, less_func);
}

// max heap interfaces
template<
    class VecT,
    class KeyFunc>
void max_heap_make(
    VecT                &heap,
    UINT8               n,
    KeyFunc             key_func) {
    
    min_heap_internal::make<0>(heap, n, key_func,
            min_heap_internal::DefaultGreaterFunc<VecT, KeyFunc>());
}

template<
    class VecT,
    class KeyFunc>
void max_heap_push_down(
    VecT                &heap,
    UINT8               i,
    UINT8               n,
    KeyFunc             key_func) {
    
    min_heap_internal::push_down<0>(heap, i, n, key_func,
            min_heap_internal::DefaultGreaterFunc<VecT, KeyFunc>());
}

template<
    class VecT,
    class KeyFunc>
void max_heap_push_up(
    VecT                &heap,
    UINT8               i,
    UINT8               n,
    KeyFunc             key_func) {

    min_heap_internal::push_up<0>(heap, i, n, key_func,
            min_heap_internal::DefaultGreaterFunc<VecT, KeyFunc>());
}

template<
    class VecT,
    class KeyFunc>
void max_heap_mark_updated(
    VecT                &heap,
    UINT8               i,
    UINT8               n,
    KeyFunc             key_func) {

    min_heap_internal::mark_updated<0>(heap, i, n, key_func,
            min_heap_internal::DefaultGreaterFunc<VecT, KeyFunc>());
}

template<
    class VecT,
    class KeyFunc>
void max_heap_remove(
    VecT                &heap,
    UINT8               i,
    UINT8               n,
    KeyFunc             key_func) {

    min_heap_internal::remove<0>(heap, i, n, key_func,
            min_heap_internal::DefaultGreaterFunc<VecT, KeyFunc>());
}

template<
    class VecT,
    class VecUINT8,
    class KeyFunc,
    class IdxFunc>
void max_heap_with_inverted_index_make(
    VecT                &heap,
    VecUINT8            &inverted_index,
    UINT8               n,
    KeyFunc             key_func,
    IdxFunc             idx_func) {
    
    min_heap_internal::make_with_inverted_index<0>(
        heap, inverted_index, n, key_func, idx_func,
        min_heap_internal::DefaultGreaterFunc<VecT, KeyFunc>());
}

template<
    class VecT,
    class VecUINT8,
    class KeyFunc,
    class IdxFunc>
void max_heap_with_inverted_index_push_down(
    VecT                &heap,
    VecUINT8            &inverted_index,
    UINT8               i,
    UINT8               n,
    KeyFunc             key_func,
    IdxFunc             idx_func) {

    min_heap_internal::push_down_with_inverted_index<0>(
        heap, inverted_index, i, n, key_func, idx_func,
        min_heap_internal::DefaultGreaterFunc<VecT, KeyFunc>());
}

template<
    class VecT,
    class VecUINT8,
    class KeyFunc,
    class IdxFunc>
void max_heap_with_inverted_index_push_up(
    VecT                &heap,
    VecUINT8            &inverted_index,
    UINT8               i,
    UINT8               n,
    KeyFunc             key_func,
    IdxFunc             idx_func) {
    
    min_heap_internal::push_up_with_inverted_index<0>(
        heap, inverted_index, i, n, key_func, idx_func,
        min_heap_internal::DefaultGreaterFunc<VecT, KeyFunc>());
}

template<
    class VecT,
    class VecUINT8,
    class KeyFunc,
    class IdxFunc>
void max_heap_with_inverted_index_mark_updated(
    VecT                &heap,
    VecUINT8            &inverted_index,
    UINT8               i,
    UINT8               n,
    KeyFunc             key_func,
    IdxFunc             idx_func) {
    
    min_heap_internal::mark_updated_with_inverted_index<0>(
        heap, inverted_index, i, n, key_func, idx_func,
        min_heap_internal::DefaultGreaterFunc<VecT, KeyFunc>());
}

template<
    class VecT,
    class VecUINT8,
    class KeyFunc,
    class IdxFunc>
void max_heap_with_inverted_index_remove(
    VecT                &heap,
    VecUINT8            &inverted_index,
    UINT8               i,
    UINT8               n,
    KeyFunc             key_func,
    IdxFunc             idx_func) {
    
    min_heap_internal::remove_with_inverted_index<0>(
        heap, inverted_index, i, n, key_func, idx_func,
        min_heap_internal::DefaultGreaterFunc<VecT, KeyFunc>());
}

} // dsimpl

#endif

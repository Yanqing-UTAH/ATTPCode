#ifndef HASH_SET
#define HASH_SET

#include <unordered_set>
#include <unordered_map>
#include <type_traits>
#include <algorithm>
#include <functional>

#if defined(__GNUC__) // TODO what's the minimum version?

template<typename Value, typename KeyExtractor>
using ExtractedKeyType = std::decay_t<std::invoke_result_t<KeyExtractor, Value>>;

template<typename Value,
    typename KeyExtractor = std::__detail::_Identity,
    typename Hash = std::hash<ExtractedKeyType<Value, KeyExtractor>>,
    typename Pred = std::equal_to<ExtractedKeyType<Value, KeyExtractor>>,
    typename Alloc = std::allocator<Value>,
    typename Tr = std::__uset_traits<std::__cache_default<Value, Hash>::value>>
struct __HashSet: public std::_Hashtable<
                    ExtractedKeyType<Value, KeyExtractor>,
                    Value,
                    Alloc,
                    KeyExtractor,
                    Pred,
                    Hash,
                    std::__detail::_Mod_range_hashing,
                    std::__detail::_Default_ranged_hash,
                    std::__detail::_Prime_rehash_policy,
                    Tr> {
    
    __HashSet(
        KeyExtractor keyExtractor)
    : std::_Hashtable<
        ExtractedKeyType<Value, KeyExtractor>,
        Value,
        Alloc,
        KeyExtractor,
        Pred,
        Hash,
        std::__detail::_Mod_range_hashing,
        std::__detail::_Default_ranged_hash,
        std::__detail::_Prime_rehash_policy,
        Tr>(

        0,
        Hash(), 
        std::__detail::_Mod_range_hashing(),
        std::__detail::_Default_ranged_hash(),
        Pred(),
        keyExtractor,
        Alloc())
    {}
};

template<typename umap_t>
size_t size_of_unordered_map(umap_t &umap)
{
    return 56 + // misc bookkepping in unordered_map
        umap.bucket_count() * 8 + // _M_buckets array
        umap.size() * sizeof(std::__detail::_Hash_node<
            typename umap_t::value_type,
            std::__cache_default<
                typename umap_t::value_type,
                std::hash<typename umap_t::key_type>>::value>); // nodes
}

#else
// default version

// XXX nothing for now

#endif


#endif


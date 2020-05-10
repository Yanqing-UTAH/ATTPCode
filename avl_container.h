#ifndef AVL_CONTAINER_H
#define AVL_CONTAINER_H

#include "avl.h"
#include <functional>

namespace dsimpl {

namespace avl_container_impl {

// We define our own identity key func here because
// std::identity is not available until C++20.
template<class T>
struct ident_key {
    constexpr T&& operator()(T&& t) const noexcept {
        return std::forward<T>(t);
    };
};

template<class Value, class KeyFunc>
using KeyType = std::decay_t<std::invoke_result_t<KeyFunc, Value>>;

/*template<class Iter, class NodeType>
struct InsertReturnType {
    Iter        position;
    bool        inserted;
    NodeType    node;
}; */

template<class Value, class RankType, class = void>
struct Node {
    Value           m_value;
    
    int             m_hdiff;

    Node            *m_left;

    Node            *m_right;
    
    Node            *m_parent;
};

template<class Value, class RankType>
struct Node<Value, RankType,
    // keep in sync with class avl_container
    std::enable_if_t<std::is_arithmetic_v<RankType>>>
{
    typedef RankType rank_type;

    Value           m_value;
    
    int             m_hdiff;
    
    rank_type       m_subtree_cnt;

    Node            *m_left;

    Node            *m_right;
    
    Node            *m_parent;
};

/*template<class Value, class RankType, class KeyType, class = void>
struct NodeDescAggBase:
    public avl_impl::AVLNodeDescBase<Node<Value, RankType>, KeyType> {

    typedef Node<Value, RankType> _node_t;
};

template<class Value, class RankType, class KeyType>
struct NodeDescAggBase<Value, RankType, KeyType,
    decltype(((Node<Value, RankType>*) nullptr)->m_subtree_cnt)>:
    public avl_impl::AVLNodeDescBase<Node<Value, RankType>, KeyType> {
    
    typedef Node<Value, RankType> _node_t;

    void _fix_agg(_node_t *n) const {
        n->m_subtree_cnt = 1;
        if (n->m_left) n->m_subtree_cnt += n->m_left->m_subtree_cnt;
        if (n->m_right) n->m_subtree_cnt += n->m_right->m_subtree_cnt;
    }
}; */

template<class Value, class RankType, class KeyFunc>
struct NodeDescBase:
    public avl_impl::AVLNodeDescBase<Node<Value, RankType>, KeyType<Value, KeyFunc>> {
    
    typedef typename avl_impl::AVLNodeDescBase<
        Node<Value, RankType>, KeyType<Value, KeyFunc>>::DATUM key_type;
    
    NodeDescBase(const KeyFunc &key_func): m_key_func(key_func) {}

    KeyFunc m_key_func;

    constexpr decltype(auto) _value2key(const Value &v) const {
        return m_key_func(v);
    }

    void swap(NodeDescBase &other) noexcept {
        swap(m_key_func, other.m_key_func);
    }
};

template<class Value, class RankType>
struct NodeDescBase<Value, RankType, ident_key<Value>>:
    public avl_impl::AVLNodeDescBase<Node<Value, RankType>, Value> {
    
    NodeDescBase(ident_key<Value>) {}

    static constexpr const Value &_value2key(const Value& v) { return v; }

    void swap(NodeDescBase &other) noexcept {}
};

template<class Value, class RankType, class KeyFunc, class Compare>
struct NodeDescBase2:
    public NodeDescBase<Value, RankType, KeyFunc> {
public:
    typedef Node<Value, RankType> _node_t;

    NodeDescBase2(const Compare &compare, const KeyFunc &key_func):
        NodeDescBase<Value, RankType, KeyFunc>(key_func),
        m_less_fn(compare) {}
    
    constexpr decltype(auto) _key(const _node_t *n) const {
        return this->_value2key(n->m_value);
    }

    constexpr static _node_t *&_left(_node_t *n) {
        return n->m_left;
    }

    constexpr static _node_t *&_right(_node_t *n) {
        return n->m_right;
    }

    constexpr static _node_t *&_parent(_node_t *n) {
        return n->m_parent;
    }

    constexpr static INT4 &_hdiff(_node_t *n) {
        return n->m_hdiff;
    }

    Compare m_less_fn;

    void swap(NodeDescBase2 &other) noexcept {
        ((NodeDescBase<Value, RankType, KeyFunc>*) this)->swap(other);
        swap(m_less_fn, other.m_less_fn);
    }
};

template<class Value, class RankType, class KeyFunc, class Compare, class = void>
struct NodeDesc:
    public NodeDescBase2<Value, RankType, KeyFunc, Compare> {

    typedef NodeDescBase2<Value, RankType, KeyFunc, Compare> Base2;

    NodeDesc(const Compare &compare, const KeyFunc &key_func):
        Base2(compare, key_func) {}

    using Base2::_key;
    using Base2::_left;
    using Base2::_right;
    using Base2::_parent;
    using Base2::_hdiff;

    void swap(NodeDesc &other) noexcept {
        ((NodeDescBase2<Value, RankType, KeyFunc, Compare>*) this)->swap(other);
    }
};

template<class Value, class RankType, class KeyFunc, class Compare>
struct NodeDesc<Value, RankType, KeyFunc, Compare,
    std::enable_if_t<std::is_arithmetic_v<RankType>>>:
    public NodeDescBase2<Value, RankType, KeyFunc, Compare> {

    typedef NodeDescBase2<Value, RankType, KeyFunc, Compare> Base2;

    NodeDesc(const Compare &compare, const KeyFunc &key_func):
        Base2(compare, key_func) {}

    using Base2::_key;
    using Base2::_left;
    using Base2::_right;
    using Base2::_parent;
    using Base2::_hdiff;

    using typename Base2::_node_t;

    void swap(NodeDesc &other) noexcept {
        ((NodeDescBase2<Value, RankType, KeyFunc, Compare>*) this)->swap(other);
    }

    static void _fix_agg(_node_t *n) {
        n->m_subtree_cnt = 1;
        if (n->m_left) n->m_subtree_cnt += n->m_left->m_subtree_cnt;
        if (n->m_right) n->m_subtree_cnt += n->m_right->m_subtree_cnt;
    }
};

template<class Value, class KeyFunc, class Compare>
struct ValueCompare: public Compare {
    template<class RankType>
    ValueCompare(const NodeDesc<Value, RankType, KeyFunc, Compare> &desc):
        Compare(desc.m_less_fn), m_key(desc.m_key_func) {}

    constexpr bool operator()(const Value &lhs, const Value &rhs) const {
        return ((Compare&)*this)(m_key(lhs), m_key(rhs));
    }

private: 
    KeyFunc m_key;
};

template<class Value, class Compare>
struct ValueCompare<Value, ident_key<Value>, Compare>: public Compare {
    template<class RankType>
    ValueCompare(
        const NodeDesc<Value, RankType, ident_key<Value>, Compare> &desc):
        Compare(desc.m_less_fn) {}

    constexpr bool operator()(const Value &lhs, const Value &rhs) const {
        return ((Compare&)*this)(lhs, rhs);
    }
};

template<class T>
struct has_is_transparent {

    template<class U>
    static int test(typename U::is_transparent*) { return 1; }

    template<class U>
    static void test(...) { }

    static constexpr const bool value = std::is_same_v<decltype(test<T>(nullptr)), int>;
};

template<
    class Value,
    class KeyFunc,
    class Compare = std::less<KeyType<Value, KeyFunc>>,
    class RankType = void, // subtree count enabled if it is arithmetic
    class Allocator = std::allocator<Value>>
class avl_container {
private:
    static constexpr const bool m_enable_rank_tree_interfaces =
        std::is_arithmetic_v<RankType>; // keep in sync with struct Node

    typedef NodeDesc<Value, RankType, KeyFunc, Compare>
                                                        _node_desc_t;
    typedef avl_t<_node_desc_t, false>                  _avl_t;
    typedef Node<Value, RankType>                       _node_t;

    class iterator_t;
    class reverse_iterator_t;
    class node_handle;

public:
    typedef KeyType<Value, KeyFunc>                     key_type;
    typedef Value                                       value_type;
    typedef std::size_t                                 size_type;
    typedef std::ptrdiff_t                              difference_type;
    typedef Compare                                     key_compare;
    typedef ValueCompare<Value, KeyFunc, Compare>       value_compare; 
    typedef Allocator                                   allocator_type;
    typedef value_type&                                 reference;
    typedef const value_type&                           const_reference;
    typedef typename std::allocator_traits<Allocator>::pointer
                                                        pointer;
    typedef typename std::allocator_traits<Allocator>::const_pointer
                                                        const_pointer;
    typedef iterator_t                                  iterator;
    typedef iterator                                    const_iterator;
    typedef reverse_iterator_t                          reverse_iterator;
    typedef reverse_iterator                            const_reverse_iterator;
    typedef node_handle                                 node_type;

    typedef RankType                                    rank_type;

private:
    typedef typename std::allocator_traits<allocator_type>::
        template rebind_alloc<_node_t>                  node_allocator_type;
    typedef std::allocator_traits<allocator_type>       value_allocator_traits;
    typedef std::allocator_traits<node_allocator_type>  node_allocator_traits;
                                                    

    static constexpr const bool m_compare_has_is_transparent =
        has_is_transparent<key_compare>::value;
    static constexpr const bool m_enable_key_type_args =
        !std::is_same_v<key_type, value_type>;

    class iterator_t {
    public: 
        typedef typename avl_container::value_type              value_type;
        typedef typename avl_container::difference_type         difference_type;
        typedef typename avl_container::reference               reference;
        typedef typename avl_container::pointer                 pointer;
        typedef typename std::bidirectional_iterator_tag        iterator_category;
        constexpr iterator_t():
            m_node(nullptr),
            m_avl(nullptr) {}

        constexpr iterator_t(const iterator_t &other):
            m_node(other.m_node),
            m_avl(other.m_avl) {}

        constexpr iterator_t(const reverse_iterator_t &other):
            m_node(other.m_node),
            m_avl(other.m_avl) {
            
            // iterator_t iter = ...;
            // iterator_t iter2(reverse_iterator_t(iter));
            // assert(iter == iter2);
            m_node = m_avl->next(m_node);
        }

        constexpr iterator_t &operator=(const iterator_t &other) {
            m_node = other.m_node;
            m_avl = other.m_avl;
            return *this;
        }

        constexpr bool operator==(const iterator_t &other) const {
            return m_node == other.m_node;
        }

        constexpr bool operator!=(const iterator_t &other) const {
            return m_node != other.m_node;
        }
        
        constexpr reference operator*() const {
            return m_node->m_value;
        }

        constexpr pointer operator->() const {
            return &m_node->m_value;
        }

        iterator_t &operator++() {
            m_node = m_avl->next(m_node);
            return *this;
        }

        iterator_t operator++(int) {
            iterator_t iter = iterator_t(*this);
            m_node = m_avl->next(m_node);
            return iter;
        }

        iterator_t &operator--() {
            m_node = m_avl->prev(m_node);
            return *this;
        }

        iterator_t operator--(int) {
            iterator_t iter = iterator_t(*this);
            m_node = m_avl->prev(m_node);
            return iter;
        }

    private:
        iterator_t(
            _node_t             *node,
            const _avl_t        *avl):
            m_node(node),
            m_avl(avl) {}

        _node_t                 *m_node;
        const _avl_t            *m_avl;

        friend class avl_container;
        friend class reverse_iterator_t;
    };

    class reverse_iterator_t {
    public: 
        typedef typename avl_container::value_type              value_type;
        typedef typename avl_container::difference_type         difference_type;
        typedef typename avl_container::reference               reference;
        typedef typename avl_container::pointer                 pointer;
        typedef typename std::bidirectional_iterator_tag        iterator_category;

        constexpr reverse_iterator_t():
            m_node(nullptr),
            m_avl(nullptr) {}

        constexpr reverse_iterator_t(const reverse_iterator_t &other):
            m_node(other.m_node),
            m_avl(other.m_avl) {}

        constexpr reverse_iterator_t(const iterator_t &other):
            m_node(other.m_node),
            m_avl(other.m_avl) {
            
            // To be consistent with std::reverse_iterator
            m_node = m_avl->prev(m_node);
        }

        constexpr reverse_iterator_t &operator=(const reverse_iterator_t &other) {
            m_node = other.m_node;
            m_avl = other.m_avl;
            return *this;
        }

        constexpr bool operator==(const reverse_iterator_t &other) const {
            return m_node == other.m_node;
        }

        constexpr bool operator!=(const reverse_iterator_t &other) const {
            return m_node != other.m_node;
        }
        
        constexpr reference operator*() const {
            return m_node->m_value;
        }

        constexpr pointer operator->() const {
            return &m_node->m_value;
        }

        reverse_iterator_t &operator++() {
            m_node = m_avl->prev(m_node);
            return *this;
        }

        reverse_iterator_t operator++(int) {
            reverse_iterator_t iter = reverse_iterator_t(*this);
            m_node = m_avl->prev(m_node);
            return iter;
        }

        reverse_iterator_t &operator--() {
            m_node = m_avl->next(m_node);
            return *this;
        }

        reverse_iterator_t operator--(int) {
            reverse_iterator_t iter = reverse_iterator_t(*this);
            m_node = m_avl->next(m_node);
            return iter;
        }

    private:
        reverse_iterator_t(
            _node_t             *node,
            const _avl_t        *avl):
            m_node(node),
            m_avl(avl) {}

        _node_t                 *m_node;
        const _avl_t            *m_avl;

        friend class avl_container;
        friend class iterator_t;
    };
    
    // the STL node handle introduced in C++17
    class node_handle {
    public:
        typedef avl_container::value_type               value_type;
        typedef avl_container::allocator_type           allocator_type;

        constexpr node_handle() noexcept:
            m_node_alloc(),
            m_node(nullptr)
        {}

        node_handle(node_handle &&nh) noexcept:
            m_node_alloc(std::move(nh.m_node_alloc)),
            m_node(nh.m_node) {

            nh.m_node = nullptr;
        }

        ~node_handle() {
            if (m_node) {
                destroy_node(m_node_alloc, m_node);
            }
        }

        node_handle &operator=(node_handle &&nh) {
            if (m_node) destroy_node(m_node_alloc, m_node);
            m_node_alloc = std::move(nh.m_node_alloc);
            m_node = nh.m_node;
            nh.m_node = nullptr;
        }

        bool empty() const noexcept {
            return !m_node;
        }

        explicit operator bool() const noexcept {
            return m_node;
        }

        allocator_type get_allocator() const {
            return allocator_type(m_node_alloc);
        }

        value_type &value() const {
            return m_node->m_value;
        }

        void swap(node_handle &nh) noexcept(
                value_allocator_traits::propagate_on_container_swap::value ||
                value_allocator_traits::is_always_equal::value){
            if (empty() || nh.empty() ||
                value_allocator_traits::propagate_on_container_swap::value) {
                swap(m_node_alloc, nh.m_node_alloc);
            }
            swap(m_node, nh.m_node);
        }

    private:
        constexpr node_handle(
            const node_allocator_type   &alloc,
            _node_t                 *node):
            m_node_alloc(alloc),
            m_node(node)
        {}
        
        node_allocator_type     m_node_alloc;

        _node_t             *m_node;

        friend class avl_container;
    };

public:
    avl_container():
        m_node_alloc(),
        m_cnt(0),
        m_avl(_node_desc_t(Compare(), KeyFunc()))
    {
    }

    explicit avl_container(
        const Compare &comp,
        const Allocator &alloc = Allocator(),
        const KeyFunc &key_func = KeyFunc()):
        m_node_alloc(alloc),
        m_cnt(0),
        m_avl(_node_desc_t(comp, key_func))
    {}

    explicit avl_container(
        const Allocator &alloc):
        m_node_alloc(alloc),
        m_cnt(0),
        m_avl(_node_desc_t(Compare(), KeyFunc()))
    {}

    explicit avl_container(
        const KeyFunc &key_func):
        m_node_alloc(),
        m_cnt(0),
        m_avl(_node_desc_t(Compare(), key_func))
    {}

    explicit avl_container(
        const KeyFunc &key_func,
        const Compare &comp):
        m_node_alloc(),
        m_cnt(0),
        m_avl(_node_desc_t(comp, key_func))
    {}

    template<class InputIt>
    avl_container(
        InputIt first,
        InputIt last,
        const Compare &comp = Compare(),
        const Allocator &alloc = Allocator(),
        const KeyFunc &key_func = KeyFunc()):
        m_node_alloc(),
        m_cnt(0),
        m_avl(_node_desc_t(comp, key_func)) {
            insert(first, last);
    }

    template<class InputIt>
    avl_container(
        InputIt first,
        InputIt last,
        const Allocator &alloc): 
        m_node_alloc(alloc),
        m_cnt(0),
        m_avl(_node_desc_t(Compare(), KeyFunc())) {
            insert(first, last);
    }

    template<class InputIt>
    avl_container(
        InputIt first,
        InputIt last,
        const KeyFunc &key_func):
        m_node_alloc(),
        m_cnt(0),
        m_avl(_node_desc_t(Compare(), key_func)) {
            insert(first, last);
    }

    avl_container(
        const avl_container &other):
        avl_container(
            other,
            value_allocator_traits::select_on_container_copy_construction(
                other.get_allocator()))
    {}

    avl_container(
        const avl_container &other,
        const Allocator &alloc):
        m_node_alloc(alloc),
        m_cnt(other.m_cnt),
        m_avl((_node_desc_t) other.m_avl) {
        
        _clone_avl(other.m_avl);
    }

    avl_container(
        avl_container &&other):
        m_node_alloc(other.get_allocator()),
        m_cnt(other.m_cnt),
        m_avl(std::move(other.m_avl)) {

        other.m_cnt = 0;
    }

    avl_container(
        avl_container &&other,
        const Allocator &alloc):
        m_node_alloc(alloc),
        m_cnt(other.m_cnt),
        m_avl((_node_desc_t) other.m_avl) {
        
        other.m_cnt = 0;
        if (m_node_alloc == other.m_node_alloc) {
            m_avl = std::move(other.m_val);
        } else {
            _clone_avl_move_values(other.m_val);
            other.m_avl.delete_tree(
                [&other](_node_t *n) {
                    node_allocator_traits::deallocate(other.m_node_alloc, n, 1); 
                });
        }
    }

    avl_container(
        std::initializer_list<value_type> init,
        const Compare &comp = Compare(),
        const Allocator &alloc = Allocator(),
        const KeyFunc &key_func = KeyFunc()):
        m_node_alloc(alloc),
        m_cnt(0),
        m_avl(_node_desc_t(comp, key_func)) {
        
        insert(init.begin(), init.end()); 
    }

    avl_container(
        std::initializer_list<value_type> init,
        const Allocator &alloc):
        m_node_alloc(alloc),
        m_cnt(0),
        m_avl(_node_desc_t(Compare(), KeyFunc())) {
        
        insert(init.begin(), init.end());
    }

    avl_container(
        std::initializer_list<value_type> init,
        const KeyFunc &key_func):
        m_node_alloc(),
        m_cnt(0),
        m_avl(_node_desc_t(Compare(), key_func)) {
        
        insert(init.begin(), init.end());
    }

    ~avl_container() {
        clear(); 
    }

    avl_container &operator=(const avl_container &other) {
        clear();
        if (value_allocator_traits::propagate_on_container_copy_assignment::value) {
            m_node_alloc = other.m_node_alloc;
        }
        m_cnt = other.m_cnt;
        _clone_avl(other.m_avl);

        return *this;
    }

    avl_container &operator=(avl_container &&other) noexcept {
        clear();
        if (value_allocator_traits::propagate_on_container_move_assignment::value) {
            m_node_alloc = other.m_node_alloc;
        }
    
        m_cnt = other.m_cnt;
        other.m_cnt = 0;
        if (m_node_alloc == other.alloc) {
            m_avl = std::move(other.m_avl);
        } else {
            _clone_avl_move_values(other.m_avl);
            other.m_avl.delete_tree(
                [&other](_node_t *n) {
                    node_allocator_traits::deallocate(other.m_node_alloc, n, 1); 
                });
        }

        return *this;
    }

    avl_container &operator=(std::initializer_list<value_type> ilist) {
        clear();
        insert(ilist.begin(), ilist.end()); 
        return *this;
    }

    allocator_type get_allocator() const {
        return allocator_type(m_node_alloc);
    }

    iterator begin() noexcept {
        return iterator(m_avl.begin_node(), &m_avl);
    }

    const_iterator begin() const noexcept {
        return const_iterator(m_avl.begin_node(), &m_avl);
    }

    const_iterator cbegin() const noexcept {
        return const_iterator(m_avl.begin_node(), &m_avl);
    }

    iterator end() noexcept {
        return iterator(nullptr, &m_avl);
    }

    const_iterator end() const noexcept {
        return iterator(nullptr, &m_avl);
    }

    const_iterator cend() const noexcept {
        return iterator(nullptr, &m_avl);
    }

    reverse_iterator rbegin() noexcept {
        return reverse_iterator(m_avl.rbegin_node(), &m_avl);
    }

    const_reverse_iterator rbegin() const noexcept {
        return const_reverse_iterator(m_avl.rbegin_node(), &m_avl);
    }

    const_reverse_iterator crbegin() const noexcept {
        return const_reverse_iterator(m_avl.rbegin_node(), &m_avl);
    }

    reverse_iterator rend() noexcept {
        return reverse_iterator(nullptr, &m_avl);
    }

    const_reverse_iterator rend() const noexcept {
        return const_reverse_iterator(nullptr, &m_avl);
    }

    const_reverse_iterator crend() const noexcept {
        return const_reverse_iterator(nullptr, &m_avl);
    }

    bool empty() const noexcept {
        return m_cnt == 0;
    }

    size_type size() const noexcept {
        return m_cnt;
    }

    size_type max_size() const noexcept {
        return std::min(std::numeric_limits<difference_type>::max(),
                std::numeric_limits<rank_type>::max());
    }

    void clear() noexcept {
        m_avl.delete_tree(
            [this](_node_t *n) {
                destroy_node(m_node_alloc, n);
            });
        m_cnt = 0;
    }

    iterator insert(const value_type &value) {
        _node_t *n = construct_node(m_node_alloc, value);
        m_avl.insert(n);
        ++m_cnt;
        return iterator(n, &m_avl);
    }

    iterator insert(value_type &&value) {
        _node_t *n = construct_node(m_node_alloc, std::forward<value_type>(value));
        m_avl.insert(n);
        ++m_cnt;
        return iterator(n, &m_avl);
    }
    
    // const_iterator and iterator are aliases and thus we
    // only have one version of insertion with hint
    iterator insert(
        iterator hint,
        const value_type &value) {
        
        _node_t *n = construct_node(m_node_alloc, value);
        _node_t *handle = m_avl.try_finding_handle_for_insert_with_hint(
            m_avl._value2key(value),
            hint.m_node);
        if (!handle) {
            m_avl.insert(n);
        } else {
            m_avl.insert_at(handle, n);
        }
        ++m_cnt;
        return iterator(n, &m_avl);
    }

    iterator insert(
        const_iterator hint,
        value_type &&value) {
        
        _node_t *n = constrct_node(m_node_alloc, std::forward<value_type>(value));
        _node_t *handle = m_avl.try_finding_handle_for_insert_with_hint(
            m_avl._value2key(value),
            hint.m_node);
        if (!handle) {
            m_avl.insert(n);
        } else {
            m_avl.insert_at(handle, n);
        }
        ++m_cnt;
        return iterator(n, &m_avl);
    }

    template<class InputIt>
    void insert(InputIt first, InputIt last) {
        for (; first != last; ++first) {
            insert(*first);
        }
    }

    void insert(std::initializer_list<value_type> ilist) {
        insert(ilist.begin(), ilist.end());
    }

    iterator insert(node_type &&nh) {
        if (nh.empty()) return end();
        _node_t *n = nh.m_node;
        nh.m_node = nullptr;
        m_avl.insert(n);
        ++m_cnt;
        return itereator(n, &m_avl);
    }

    iterator insert(const_iterator hint, node_type &&nh) {
        if (nh.empty()) return end();
        _node_t *n = nh.m_node;
        nh.m_node = nullptr;
        _node_t *handle = m_avl.try_finding_handle_for_insert_with_hint(
            m_avl._value2key(n->m_value),
            hint.m_node);
        if (!handle) {
            m_avl.insert(n);
        } else {
            m_avl.insert_at(handle, n);
        }
        ++m_cnt;
        return iterator(n, &m_avl);
    }

    template<class... Args>
    iterator emplace(Args&&... args) {
        _node_t *n = construct_node(m_node_alloc, std::forward<Args>(args)...);
        m_avl.insert(n);
        ++m_cnt;
        return iterator(n, &m_avl);
    }

    template<class... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args) {
        _node_t *n = construct_node(m_node_alloc, std::forward<Args>(args)...);
        _node_t *handle = m_avl.try_finding_handle_for_insert_with_hint(
            m_avl._value2key(n->m_value),
            hint.m_node);
        if (!handle) {
            m_avl.insert(n);
        } else {
            m_avl.insert_at(handle, n);
        }
        ++m_cnt;
        return iterator(n, &m_avl);
    }

    iterator erase(iterator pos) {
        _node_t *n = pos.m_node;
        _node_t *next_node = m_avl.next(n);
        m_avl.erase(n);
        --m_cnt;
        return iterator(next_node, &m_avl);
    }

    iterator erase(const_iterator first, const_iterator last) {
        while (first != last) {
            first = erase(first);
        }
        return first;
    }
    
    // If a function f supports both const value_type & and const key_type &
    // arguments and they are the same, we only enable the const value_type &
    // version by default, to be consistent with STL containers.
    // In addition, a second interface f_by_key is provided if there is
    // not a templated version of f.
    size_type erase(const value_type &value) {
        return erase_by_key(m_avl._value2key(value));
    }
    
    template<bool b = m_enable_key_type_args>
    std::enable_if_t<b, size_type>
    erase(const key_type &key) {
        return erase_by_key(key);
    }

    size_type erase_by_key(const key_type &key) {
        _node_t *n = m_avl.lower_bound_node(key);
        size_type n_removed = 0;
        while (n && !m_avl.m_less_fn(key, m_avl._key(n))) {
            _node_t *n2 = n;
            n = m_avl.next(n);
            m_avl.erase(n2);
            ++n_removed;
        }
        m_cnt -= n_removed;
        return n_removed;
    }

    void swap(avl_container &other) noexcept(
        value_allocator_traits::is_always_equal::value &&
        std::is_nothrow_swappable<Compare>::value) {
        
        using std::swap;
        if (value_allocator_traits::propagate_on_container_swap::value) {
            swap(m_node_alloc, other.m_node_alloc);
        }
        m_avl.swap(other.m_avl);
        swap(m_cnt, other.m_cnt);
    }

    node_type extract(const_iterator position) {
        _node_t *n = position.m_node;
        m_avl.erase(n);
        --m_cnt;
        return node_type(m_node_alloc, n);
    }

    node_type extract(const value_type &x) {
        return extract_by_key(m_avl._value2key(x));
    }
    
    template<bool b = m_enable_key_type_args>
    std::enable_if_t<b, node_type>
    extract(const key_type &x) {
        return extract_by_key(x);
    }

    node_type extract_by_key(const key_type &x) {
        _node_t *n = m_avl.lower_bound_node(x);
        if (!n) {
            return node_type();
        }
        
        m_avl.erase(n);
        --m_cnt;
        return node_type(m_node_alloc, n);
    }

    template<class C2>
    void merge(avl_container<Value, KeyFunc, RankType, C2, Allocator> &source) {
        source.m_avl.delete_tree(
            [this](_node_t *n) {
                m_avl.insert(n);
            });
        m_cnt += source.m_cnt;
        source.m_cnt = 0;
    }

    template<class C2>
    void merge(avl_container<Value, KeyFunc, RankType, C2, Allocator> &&source) {
        merge(source);
    }

    size_type count(const value_type &value) const {
        return count<key_type>(m_avl._value2key(value));
    }

    size_type count_by_key(const key_type &key) const {
        return count<key_type>(key);
    }

    template<class K>
    std::enable_if_t<m_compare_has_is_transparent ||
        std::is_same_v<K, key_type>, size_type>
    count(const K& key) const {
        _node_t *n = m_avl.lower_bound_node(key);
        size_type cnt = 0;
        while (n && !m_avl.m_less_fn(key, m_avl._key(n))) {
            ++cnt;
            n = m_avl.next(n);
        }
        return cnt;
    }

    iterator find(const value_type &value) {
        return find<key_type>(m_avl._value2key(value));
    }

    const_iterator find(const value_type &value) const {
        return find<key_type>(m_avl._value2key(value));
    }

    const_iterator find_by_key(const key_type &key) const {
        return find<key_type>(key);
    }

    template<class K>
    std::enable_if_t<m_compare_has_is_transparent ||
        std::is_same_v<K, key_type>, iterator>
    find(const K &key) {
        return ((const avl_container*) this)->find<K>(key);
    }

    template<class K>
    std::enable_if_t<m_compare_has_is_transparent ||
        std::is_same_v<K, key_type>, const_iterator>
    find(const K &key) const {
        _node_t *n = m_avl.find_node(key);
        return const_iterator(n, &m_avl);
    }

    std::pair<iterator, iterator> equal_range(const value_type &value) {
        return equal_range<key_type>(m_avl._value2key(value));
    }

    std::pair<const_iterator, const_iterator> equal_range(
        const value_type &value) const {
    
        return equal_range<key_type>(m_avl._value2key(value));
    }

    std::pair<const_iterator, const_iterator> equal_range_by_key(
        const key_type &key) const {

        return equal_range<key_type>(key);
    }
    
    template<class K>
    std::enable_if_t<m_compare_has_is_transparent ||
        std::is_same_v<K, key_type>, std::pair<iterator, iterator>>
    equal_range(const K &key) {
        return ((const avl_container*)this)->equal_range<K>(key);
    }

    template<class K>
    std::enable_if_t<m_compare_has_is_transparent ||
        std::is_same_v<K, key_type>, std::pair<const_iterator, const_iterator>>
    equal_range(const K &key) const {
        _node_t *lb = m_avl.lower_bound_node(key);
        _node_t *ub = m_avl.upper_bound_node(key);
        return std::make_pair(const_iterator(lb, &m_avl), const_iterator(ub, &m_avl));
    }

    iterator lower_bound(const value_type &value) {
        return lower_bound<key_type>(m_avl._value2key(value));
    }

    const_iterator lower_bound(const value_type &value) const {
        return lower_bound<key_type>(m_avl._value2key(value));
    }

    const_iterator lower_bound_by_key(const key_type &key) const {
        return lower_bound<key_type>(key);
    }

    template<class K>
    std::enable_if_t<m_compare_has_is_transparent ||
        std::is_same_v<K, key_type>, iterator>
    lower_bound(const K &key) {
        return ((const avl_container *) this)->lower_bound<K>(key);
    }

    template<class K>
    std::enable_if_t<m_compare_has_is_transparent ||
        std::is_same_v<K, key_type>, const_iterator>
    lower_bound(const K &key) const {
        return const_iterator(m_avl.lower_bound_node(key), &m_avl);
    }

    iterator upper_bound(const value_type &value) {
        return upper_bound<key_type>(m_avl._value2key(value));
    }

    const_iterator upper_bound(const value_type &value) const {
        return upper_bound<key_type>(m_avl._value2key(value));
    }

    const_iterator upper_bound_by_key(const key_type &key) const {
        return upper_bound<key_type>(key);
    }

    template<class K>
    std::enable_if_t<m_compare_has_is_transparent ||
        std::is_same_v<K, key_type>, iterator>
    upper_bound(const K &key) {
        return ((const avl_container *) this)->upper_bound<K>(key);
    }

    template<class K>
    std::enable_if_t<m_compare_has_is_transparent ||
        std::is_same_v<K, key_type>, const_iterator>
    upper_bound(const K &key) const {
        return const_iterator(m_avl.upper_bound_node(key), &m_avl);
    }

    key_compare key_comp() const {
        return m_avl.m_less_fn;
    }

    value_compare value_comp() const {
        return ValueCompare((const _node_desc_t&) m_avl);
    } 

// rank tree interfaces
// enabled only when m_enable_rank_tree_interfaces == true

    template<bool b = m_enable_rank_tree_interfaces>
    std::enable_if_t<b, rank_type>
    get_rank(const_iterator iter) const {
        rank_type rank = 0;
        m_avl.template get_sum_left<false>(
            iter.m_node,
            rank,
            [](rank_type &rank, _node_t *n) {
                rank += n->m_subtree_cnt;
            },
            [](rank_type &rank, _node_t *n) {
                rank -= n->m_subtree_cnt;
            });
        return rank;
    }

    template<bool b = m_enable_rank_tree_interfaces>
    const_iterator get_nth(
        std::enable_if_t<b, rank_type> n) const {
        
        _node_t *node = m_avl.get_nth_node(
            n,
            [](rank_type &n, _node_t *node) -> bool {
                if (n < node->m_subtree_cnt) return false;
                n -= node->m_subtree_cnt;
                return true;
            },
            [](rank_type &n, _node_t*) -> bool {
                if (n == 0) return false;
                --n;
                return true;
            });
        return const_iterator(node, &m_avl);
    }

private:
    void _clone_avl(const _avl_t &other_avl) {
        m_avl.clone_from(
            other_avl,
            [this](_node_t *orig) -> _node_t *{
                return construct_node(m_node_alloc, orig->m_value);
            });
    }

    void _clone_avl_move_values(const _avl_t &other_avl) {
        m_avl.clone_from(
            other_avl,
            [this](_node_t *orig) -> _node_t *{
                _node_t *n = node_allocator_traits::allocate(m_node_alloc, 1);
                node_allocator_traits::construct(
                    m_node_alloc, &n->m_value, std::move(orig->m_value));
                return n;
            });
    }
    
    template<class... Args>
    static _node_t *construct_node(
        node_allocator_type &alloc,
        Args&&... args) {

        _node_t *n = node_allocator_traits::allocate(alloc, 1);
        node_allocator_traits::construct(
            alloc, &n->m_value, std::forward<Args>(args)...);
        return n;
    }

    static void destroy_node(
        node_allocator_type &alloc,
        _node_t *n) {
        
        node_allocator_traits::destroy(alloc, &n->m_value);
        node_allocator_traits::deallocate(alloc, n, 1);
    }

    node_allocator_type         m_node_alloc;

    size_type                   m_cnt;

    _avl_t                      m_avl;
};

template<
    class Value,
    class KeyFunc,
    class Compare,
    class Allocator>
void swap(
    typename avl_container<Value, KeyFunc, Compare, Allocator>::node_type &x,
    typename avl_container<Value, KeyFunc, Compare, Allocator>::node_type &y)
    noexcept(noexcept(x.swap(y))) {
    
    x.swap(y);
}

template<class Value, class KeyFunc, class Compare, class Allocator>
bool operator==(
    const avl_container<Value, KeyFunc, Compare, Allocator> &lhs,
    const avl_container<Value, KeyFunc, Compare, Allocator> &rhs) {
    
    return std::equal(
        lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template<class Value, class KeyFunc, class Compare, class Allocator>
bool operator!=(
    const avl_container<Value, KeyFunc, Compare, Allocator> &lhs,
    const avl_container<Value, KeyFunc, Compare, Allocator> &rhs) {
    
    return !(lhs == rhs);
}

template<class Value, class KeyFunc, class Compare, class Allocator>
bool operator<(
    const avl_container<Value, KeyFunc, Compare, Allocator> &lhs,
    const avl_container<Value, KeyFunc, Compare, Allocator> &rhs) {
    
    return std::lexicographical_compare(
        lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template<class Value, class KeyFunc, class Compare, class Allocator>
bool operator>(
    const avl_container<Value, KeyFunc, Compare, Allocator> &lhs,
    const avl_container<Value, KeyFunc, Compare, Allocator> &rhs) {
    
    return std::lexicographical_compare(
        lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), std::greater<Value>());
}

template<class Value, class KeyFunc, class Compare, class Allocator>
bool operator<=(
    const avl_container<Value, KeyFunc, Compare, Allocator> &lhs,
    const avl_container<Value, KeyFunc, Compare, Allocator> &rhs) {
    
    return !(lhs > rhs);
}

template<class Value, class KeyFunc, class Compare, class Allocator>
bool operator>=(
    const avl_container<Value, KeyFunc, Compare, Allocator> &lhs,
    const avl_container<Value, KeyFunc, Compare, Allocator> &rhs) {
    
    return !(lhs < rhs);
}

template<class Value, class KeyFunc, class Compare, class Allocator>
void swap(
    avl_container<Value, KeyFunc, Compare, Allocator> &lhs,
    avl_container<Value, KeyFunc, Compare, Allocator> &rhs)
    noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

// deduction guides for avl_container
template<class Ret, class Arg>
struct infer_value_type_from_key_func {
    infer_value_type_from_key_func(std::function<Ret(Arg)> f) {}

    typedef std::decay_t<Arg> value_type;
};

template<typename KeyFunc>
explicit avl_container(KeyFunc f) ->
avl_container<
    typename decltype(infer_value_type_from_key_func(std::function(f)))::value_type,
    KeyFunc>;

template<typename KeyFunc, typename Compare>
explicit avl_container(KeyFunc f, Compare) ->
avl_container<
    typename decltype(infer_value_type_from_key_func(std::function(f)))::value_type,
    KeyFunc,
    Compare>;

template<
    class InputIt,
    class Compare = std::less<typename std::iterator_traits<InputIt>::value_type>,
    class Alloc = std::allocator<typename std::iterator_traits<InputIt>::value_type>,
    class KeyFunc = ident_key<typename std::iterator_traits<InputIt>::value_type>>
avl_container(InputIt, InputIt,
    Compare = Compare(), Alloc = Alloc(), KeyFunc = KeyFunc()) ->
avl_container<
    typename std::iterator_traits<InputIt>::value_type,
    KeyFunc,
    Compare,
    void,
    Alloc>;

/*template<class InputIt, class Alloc>
avl_container(InputIt, InputIt, Alloc> ->
avl_container<
    typename std::iterator_traits<InputIt>::value_type,
    ident_key<typename std::iterator_traits<InputIt>::value_type>,
    std::less<typename std::iterator_traits<InputIt>::value_type>,
    void,
    Alloc>; */

template<class InputIt, class KeyFunc>
avl_container(InputIt, InputIt, KeyFunc) ->
avl_container<
    typename std::iterator_traits<InputIt>::value_type,
    KeyFunc,
    std::less<typename std::iterator_traits<InputIt>::value_type>,
    void,
    std::allocator<typename std::iterator_traits<InputIt>::value_type>>;

template<
    class Value,
    class Compare = std::less<Value>,
    class Alloc = std::allocator<Value>,
    class KeyFunc = ident_key<Value>>
avl_container(
    std::initializer_list<Value>,
    Compare = Compare(),
    Alloc = Alloc(),
    KeyFunc = KeyFunc()) ->
avl_container<
    Value,
    KeyFunc,
    Compare,
    void,
    Alloc>;

template<class Value, class KeyFunc>
avl_container(std::initializer_list<Value>, KeyFunc) ->
avl_container<
    Value,
    KeyFunc,
    std::less<Value>,
    void,
    std::allocator<Value>>;

} // namespace avl_container_impl

using avl_container_impl::avl_container;

template<
    class Value,
    class Compare = std::less<Value>,
    class Allocator = std::allocator<Value>>
using avl_multiset = avl_container_impl::avl_container<
    Value, avl_container_impl::ident_key<Value>, Compare, void, Allocator>;

template<
    class Value,
    class Compare = std::less<Value>,
    class Allocator = std::allocator<Value>>
using avl_ranked_multiset = avl_container_impl::avl_container<
    Value, avl_container_impl::ident_key<Value>, Compare,
    typename avl_multiset<Value, Compare, Allocator>::size_type,
    Allocator>;

} // namespace dsimpl

#endif // AVL_CONTAINER_H


//
//! Copyright © 2013-2018
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
//! Original implementation from "The Art of Multiprocessor Programming",
//! @book{Herlihy:2008 : AMP : 1734069,
//! author = { Herlihy, Maurice and Shavit, Nir },
//! title = { The Art of Multiprocessor Programming },
//! year = { 2008 },
//! isbn = { 0123705916, 9780123705914 },
//! publisher = { Morgan Kaufmann Publishers Inc. },
//! address = { San Francisco, CA, USA },
//}
#ifndef STK_CONTAINER_LOCK_FREE_CONCURRENT_SKIP_LIST_HPP
#define STK_CONTAINER_LOCK_FREE_CONCURRENT_SKIP_LIST_HPP
#pragma once

#include <stk/container/experimental/lazy_concurrent_skip_list.hpp>
#include <stk/thread/concurrentqueue.h>
#include <stk/container/atomic_markable_ptr.hpp>
#include <stk/thread/backoff.hpp>
#include <stk/thread/std_thread_kernel.hpp>
#include <stk/container/quiesce_memory_reclaimer.hpp>
#include <memory>

//! A lock-free concurrent skip list implementation with map and set versions.
namespace stk { namespace detail {
    template<typename AssociativeTraits>
    class lf_skip_list_node_manager : public AssociativeTraits
    {
    protected:
        struct          node;
        friend struct   node;

        using node_ptr = node*;
        using traits = AssociativeTraits;
        using allocator_type = typename traits::allocator_type;
        using key_compare = typename traits::key_compare;
        using value_type = typename traits::value_type;
        using key_type = typename traits::key_type;
        using mutex_type = typename traits::mutex_type;
        using atomic_ptr = atomic_markable_ptr<node>;
        using mark_type = typename atomic_ptr::mark_type;

        struct node
        {
            enum flag : std::uint8_t
            {
                Head = 1
              , MarkedForRemoval = (1 << 1)
            };

            std::uint8_t     get_flags() const { return flags.load(/*std::memory_order_consume*/std::memory_order_acquire); }
            void             set_flags(std::uint8_t f) { flags.store(f, std::memory_order_release); }
            bool             is_head() const { return get_flags() & flag::Head; }
            bool             is_marked_for_removal() const { return get_flags() & flag::MarkedForRemoval; }
            void             set_is_head() { set_flags(std::uint8_t(get_flags() | flag::Head)); }
            void             set_marked_for_removal() { set_flags(std::uint8_t(get_flags() | flag::MarkedForRemoval)); }
            const key_type&  key() const { return traits::resolve_key(value_); }
            value_type&      item() { return value_; }
            template <typename Value>
            void             set_value(Value&& v){ value_ = std::forward<Value>(v); }
            atomic_ptr&      next(std::uint8_t i) { GEOMETRIX_ASSERT(i <= topLevel); return nexts[i]; }
            void             set_next(std::uint8_t i, node_ptr pNode, mark_type mark) { GEOMETRIX_ASSERT(i <= topLevel); return nexts[i].store(pNode, mark, std::memory_order_release); }
            std::uint8_t     get_top_level() const { return topLevel; }

            std::int8_t      get_num_links() const { return num_links.load(std::memory_order_acquire); }
            bool             unlink() { return num_links.fetch_sub(1, std::memory_order_acq_rel) == 1; }

        private:

            friend class lf_skip_list_node_manager<traits>;

            template <typename Value>
            node(std::uint8_t topLevel, Value&& data, bool isHead)
                : value_(std::forward<Value>(data))
                , flags(!isHead ? 0 : 0 | flag::Head)
                , num_links(topLevel+1)
                , topLevel(topLevel)
            {
                GEOMETRIX_ASSERT(isHead == is_head());
                GEOMETRIX_ASSERT(!is_marked_for_removal());
                for (std::uint8_t i = 0; i <= topLevel; ++i)
                    new(&nexts[i]) atomic_markable_ptr<node>(nullptr, 0);
            }

            value_type                 value_;
            std::atomic<std::uint8_t>  flags;
            std::atomic<std::int8_t>   num_links;
            std::uint8_t               topLevel;
            atomic_markable_ptr<node>  nexts[0];//! C trick for dynamic arrays all with one allocation.
        };

        class node_scope_manager
        {
        public:

            node_scope_manager(allocator_type al)
                : m_nodeAllocator(al)
            {}

            ~node_scope_manager()
            {}

            template <typename Data>
            node_ptr create_node(Data&& v, std::uint8_t topLevel, bool isHead = false)
            {
                std::size_t aSize = sizeof(node) + (topLevel + 1) * sizeof(atomic_markable_ptr<node>);
                auto pNode = reinterpret_cast<node_ptr>(m_nodeAllocator.allocate(aSize));
                new(pNode) node(topLevel, std::forward<Data>(v), isHead);
                return pNode;
            }

            void register_node_for_deletion(node_ptr pNode)
            {
                m_reclaimer.add([pNode, this]() { delete_node(pNode); });
            }

            void add_checkout(node_ptr pNode=nullptr)
            {
                m_reclaimer.add_checkout(pNode);
            }

            void remove_checkout(node_ptr pNode=nullptr)
            {
                m_reclaimer.remove_checkout(pNode);
            }

            void quiesce()
            {
                m_reclaimer.quiesce();
            }

            void delete_node(node_ptr pNode)
            {
                std::size_t aSize = sizeof(node) + (pNode->get_top_level() + 1) * sizeof(atomic_markable_ptr<node>);
                pNode->~node();
                m_nodeAllocator.deallocate((std::uint8_t*)pNode, aSize);
            }

        private:

            using node_allocator = typename allocator_type::template rebind<std::uint8_t>::other;   // allocator object for nodes is a byte allocator.
            node_allocator m_nodeAllocator;
            stk::thread::quiesce_memory_reclaimer m_reclaimer;
        };

        lf_skip_list_node_manager(key_compare p, allocator_type al)
            : AssociativeTraits(al)
            , m_compare(p)
            , m_scopeManager(std::make_shared<node_scope_manager>(al))
        {}

        template <typename Data>
        node_ptr create_node(Data&& v, std::uint8_t topLevel, bool isHead = false)
        {
            return m_scopeManager->create_node(std::forward<Data>(v), topLevel, isHead);
        }

        void register_node_for_deletion(node_ptr pNode)
        {
            m_scopeManager->register_node_for_deletion(pNode);
        }

        void delete_node(node_ptr pNode)
        {
            m_scopeManager->delete_node(pNode);
        }

        key_compare key_comp() const { return this->m_compare; }
        bool less(node_ptr pNode, const key_type& k) const
        {
            return pNode->is_head() || key_comp()(pNode->key(), k);
        }

        bool equal(node_ptr pNode, const key_type& k) const
        {
            return !pNode->is_head() && !key_comp()(pNode->key(), k) && !key_comp()(k, pNode->key());
        }

        template <typename U, typename Alloc>
        static U* allocate(Alloc& al, std::size_t n)
        {
            return typename std::allocator_traits<Alloc>::template rebind_alloc<U>(al).allocate(n);
        }

        template <typename U, typename Alloc>
        static void deallocate(Alloc& al, U* u, std::size_t n)
        {
            typename std::allocator_traits<Alloc>::template rebind_alloc<U>(al).deallocate(u, n);
        }

        std::shared_ptr<node_scope_manager> get_scope_manager() const { return m_scopeManager; }

    private:
        key_compare m_compare;
        std::shared_ptr<node_scope_manager> m_scopeManager;
    };
}//! namespace detail;

template <typename AssociativeTraits, typename LevelSelectionPolicy = skip_list_level_selector<AssociativeTraits::max_level::value+1>, typename BackoffPolicy = stk::thread::backoff_policy<stk::thread::std_thread_traits, stk::thread::exp_backoff_policy>>
class lock_free_concurrent_skip_list : public detail::lf_skip_list_node_manager<AssociativeTraits>
{
    static_assert(AssociativeTraits::max_height::value > 1 && AssociativeTraits::max_height::value <= 64, "MaxHeight should be in the range [2, 64]");
    using base_type = detail::lf_skip_list_node_manager<AssociativeTraits>;
    using node_ptr = typename base_type::node_ptr;
    using node_scope_manager = typename base_type::node_scope_manager;
    using base_type::get_scope_manager;
    using base_type::create_node;
    using base_type::register_node_for_deletion;
    using base_type::delete_node;
    using base_type::less;
    using base_type::equal;
    using base_type::resolve_key;
    using mark_type = typename base_type::mark_type;
public:

    using max_level = typename base_type::max_level;
    using max_height = typename base_type::max_height;
    using traits_type = AssociativeTraits;
    using size_type = typename traits_type::size_type;
    using key_type = typename traits_type::key_type;
    using value_type = typename traits_type::value_type;
    using key_compare = typename traits_type::key_compare;
    using allocator_type = typename traits_type::allocator_type;
    using reference = typename std::add_lvalue_reference<value_type>::type;
    using const_reference = typename std::add_const<reference>::type;
    using level_selector = LevelSelectionPolicy;
    using back_off = BackoffPolicy;

    template <typename T>
    class node_iterator;

    template <typename T>
    friend class node_iterator;

    template <typename T>
    class node_iterator
        : public boost::iterator_facade
        <
            node_iterator<T>
          , T
          , boost::forward_traversal_tag
        >
    {
    public:

        friend class lock_free_concurrent_skip_list< traits_type, level_selector >;

        node_iterator()
            : m_pNode()
        {}

        node_iterator(const node_iterator& other)
            : m_pNode(other.m_pNode)
        {}

        node_iterator& operator = (const node_iterator& rhs)
        {
            m_pNode = rhs.m_pNode;
            return *this;
        }

        node_iterator(node_iterator&& other)
            : m_pNode(other.m_pNode)
        {
            other.m_pNode = nullptr;
        }

        node_iterator& operator = (node_iterator&& rhs)
        {
            m_pNode = rhs.m_pNode;
            rhs.m_pNode = nullptr;
            return *this;
        }

        explicit node_iterator(node_ptr pNode)
            : m_pNode( pNode )
        {}

        ~node_iterator()
        {}

    protected:

        template <typename U>
        BOOST_FORCEINLINE bool is_uninitialized(std::weak_ptr<U> const& weak)
        {
            using wt = std::weak_ptr<U>;
            return !weak.owner_before(wt{}) && !wt{}.owner_before(weak);
        }

        friend class boost::iterator_core_access;

        void increment()
        {
            if (m_pNode)
            {
                auto pNode = m_pNode->next(0).get_ptr();
                m_pNode = pNode;
            }
        }

        bool equal( node_iterator<T> const& other) const
        {
            return m_pNode == other.m_pNode;
        }

        //! Not thread safe.
        T& dereference() const
        {
            GEOMETRIX_ASSERT( m_pNode );
            return m_pNode->item();
        }

        node_ptr m_pNode{ nullptr };
    };

    struct const_iterator;

    struct iterator : node_iterator< value_type >
    {
        iterator()
            : node_iterator< value_type >()
        {}

        iterator(const iterator&) = default;
        iterator& operator=(const iterator&) = default;
        iterator(iterator&& o)
			: node_iterator<value_type>(std::forward<iterator>(o))
		{}
        iterator& operator=(iterator&& o)
		{
			return node_iterator<value_type>::operator =(std::forward<iterator>(o));
		}

        iterator( node_ptr pNode )
            : node_iterator< value_type >( pNode )
        {}

        template <typename U>
        bool operator ==( node_iterator<U> const& other ) const
        {
            return this->m_pNode == other.m_pNode;
        }

        template <typename U>
        bool operator !=( node_iterator<U> const& other ) const
        {
            return this->m_pNode != other.m_pNode;
        }
    };

    struct const_iterator : node_iterator< const value_type >
    {
        const_iterator()
            : node_iterator< const value_type >()
        {}

        const_iterator(const const_iterator&) = default;
        const_iterator& operator=(const const_iterator&) = default;
		const_iterator(const_iterator&& o)
			: node_iterator<value_type>(std::forward<const_iterator>(o))
		{}
		const_iterator& operator=(const_iterator&& o)
		{
			return node_iterator<value_type>::operator =(std::forward<const_iterator>(o));
		}
        const_iterator( node_ptr pNode )
            : node_iterator< const value_type >(pNode)
        {}

        const_iterator( iterator const& other )
            : node_iterator< const value_type >( other.m_pNode )
        {}

        const_iterator operator =( iterator const& other )
        {
            this->m_pNode = other.m_pNode;
            return *this;
        }

        bool operator ==( iterator const& other ) const
        {
            return this->m_pNode == other.m_pNode;
        }

        bool operator ==( const_iterator const& other ) const
        {
            return this->m_pNode == other.m_pNode;
        }

        bool operator !=( iterator const& other ) const
        {
            return this->m_pNode != other.m_pNode;
        }

        bool operator !=( const_iterator const& other ) const
        {
            return this->m_pNode != other.m_pNode;
        }
    };

    lock_free_concurrent_skip_list( std::uint8_t topLevel = 1, const key_compare& pred = key_compare(), const allocator_type& al = allocator_type() )
        : detail::lf_skip_list_node_manager<AssociativeTraits>( pred, al )
        , m_selector()
        , m_pHead(create_node(value_type(), topLevel, true))
    {
        GEOMETRIX_ASSERT( m_pHead );
    }

    ~lock_free_concurrent_skip_list()
    {
        release();
    }

    iterator begin()
    {
        return iterator(left_most());
    }

    const_iterator begin() const
    {
        return const_iterator(left_most());
    }

    iterator end()
    {
        return iterator(nullptr);
    }

    const_iterator end() const
    {
        return const_iterator(nullptr);
    }

    iterator find( const key_type& x )
    {
        std::array<node_ptr, max_height::value> preds;
        std::array<node_ptr, max_height::value> succs;

        auto found = find( x, preds, succs );
        if( found )
        {
            node_ptr pFound = succs[0];
            GEOMETRIX_ASSERT( pFound );
            if( pFound && !pFound->is_marked_for_removal() )
                return iterator(pFound);
        }

        return end();
    }

    const_iterator find(const key_type& x) const
    {
        std::array<node_ptr, max_height::value> preds;
        std::array<node_ptr, max_height::value> succs;
		using non_const_this = lock_free_concurrent_skip_list<AssociativeTraits, LevelSelectionPolicy, BackoffPolicy>*;
        auto found = const_cast<non_const_this>(this)->find( x, preds, succs );
        if( found )
        {
            auto pFound = succs[0];
            GEOMETRIX_ASSERT( pFound );
            if( pFound && !pFound->is_marked_for_removal() )
                return const_iterator(pFound);
        }

        return end();
    }

    std::pair<iterator, bool> insert(const value_type& item)
    {
        return add( item );
    }
 
	std::pair<iterator, bool> insert(value_type&& item)
    {
        return add( std::forward<value_type>(item) );
    }
 
    iterator insert(const iterator&, const value_type& item)
    {
        return add( item ).first;
    }

    iterator insert(const iterator&, value_type&& item)
    {
        return add( std::forward<value_type>(item) ).first;
    }

    bool contains( const key_type& x ) const
    {
        int bottomLevel = 0;
        mark_type mark = 0;
        node_ptr pred = m_pHead;
        node_ptr curr = nullptr;
        node_ptr succ = nullptr;
        for (int level = max_level::value; level >= bottomLevel; --level)
        {
            curr = pred->next(level).get_ptr();
            while (curr)
            {
                std::tie(succ, mark) = curr->next(level).get();
                while (mark)
                {
                    curr = curr->next(level).get_ptr();
                    if (curr)
                        std::tie(succ, mark) = curr->next(level).get();
                    else
                        break;
                }
                if (curr && less(curr, x))
                {
                    pred = curr;
                    curr = succ;
                }
                else
                    break;
            }
        }

        return curr && equal(curr, x);
    }

    iterator erase(const key_type& x)
    {
        node_ptr succ;
        int bottomLevel = 0;
        std::array<node_ptr, max_height::value> preds;
        std::array<node_ptr, max_height::value> succs;
        back_off bkoff;

        while (true)
        {
            auto found = find(x, preds, succs);
            if( !found )
                return end();
            else
            {
                auto nodeToRemove = succs[bottomLevel];
                GEOMETRIX_ASSERT(nodeToRemove);
                for(int level = nodeToRemove->get_top_level(); level >= bottomLevel+1; --level)
                {
                    mark_type mark = 0;
                    std::tie(succ, mark) = nodeToRemove->next(level).get();
                    while(!mark)
                        nodeToRemove->next(level).compare_exchange_weak(succ, mark, succ, 1);
                }
                succ = nodeToRemove->next(bottomLevel).get_ptr();
                while(true)
                {
                    auto mark = mark_type{0};
                    bool iMarkedIt = nodeToRemove->next(bottomLevel).compare_exchange_strong(succ, mark, succ, 1);
                    GEOMETRIX_ASSERT(nodeToRemove == succs[bottomLevel]);
                    if(iMarkedIt)
                    {
                        nodeToRemove->set_marked_for_removal();
                        find(x, preds, succs);
                        //GEOMETRIX_ASSERT(is_unlinked(nodeToRemove));
                        //register_node_for_deletion(nodeToRemove);
                        decrement_size();
                        return iterator(succs[bottomLevel]);
                    }
                    else if(mark)
                        return end();

                    bkoff();
                }
            }
        }
    }

    iterator erase(const_iterator it)
    {
        if (it != end())
            return erase(resolve_key(*it));
        return end();
    }

    //! Clear is not thread-safe.
    void clear()
    {
        for (auto it = begin(); it != end(); ++it)
            erase(it);
    }

    //! Returns the size at any given moment. This can be volatile in the presence of writers in other threads.
    size_type size() const { return m_size.load(std::memory_order_relaxed); }

    //! Returns empty state at any given moment. This can be volatile in the presence of writers in other threads.
    bool empty() const { return left_most() == nullptr; }

    void quiesce()
    {
        scan_unlink();
        get_scope_manager()->quiesce();
    }

protected:

    node_ptr left_most() const
    {
        return m_pHead->next(0).get_ptr();
    }

    void release()
    {
        GEOMETRIX_ASSERT( m_pHead );

        auto pCurr = m_pHead;
        while (pCurr)
        {
            auto pNext = pCurr->next(0).get_ptr();
            delete_node(pCurr);
            pCurr = pNext;
        }
    }

    template <typename Preds, typename Succs>
    bool find(const key_type& key, Preds& preds, Succs& succs)
    {
        int bottomLevel = 0;
        mark_type mark = 0;
        bool snip;
        node_ptr pred = nullptr, curr = nullptr, succ = nullptr;
        retry:
        while(true)
        {
            pred = m_pHead;
            for(int level = max_level::value; level >= bottomLevel; --level)
            {
                curr = pred->next(level).get_ptr();
                while(curr)
                {
                    std::tie(succ, mark) = curr->next(level).get();
                    GEOMETRIX_ASSERT(!succ || succ->get_top_level() <= max_level::value);
                    while(mark)
                    {
                        auto expectedStamp = mark_type{ 0 };
                        snip = pred->next(level).compare_exchange_strong(curr, expectedStamp, succ, 0);
                        if(!snip)
                            goto retry;
                        GEOMETRIX_ASSERT(curr);
                        if (curr->unlink())//feeble attempt at ref counting to removal.. fails as it can be relinked concurrently at other levels.
                            register_node_for_deletion(curr);
                        std::tie(succ, mark) = curr->next(level).get();
                    }

                    if( curr && less(curr, key) )
                    {
                        pred = curr;
                        curr = succ;
                    }
                    else
                        break;
                }
                preds[level] = pred;
                succs[level] = curr;
            }
            return curr && equal(curr, key);
        }
    }

    void scan_unlink()
    {
        auto pred = m_pHead;
        while (pred)
        {
            auto predTop = pred->get_top_level();
            for(int level = 0UL; level <= predTop; ++level)
            {
                auto curr = pred->next(level).get_ptr();
                if(curr && curr->get_top_level() >= level)
                {
                    node_ptr succ;
                    mark_type mark;
                    do
                    {
                        std::tie(succ, mark) = curr->next(level).get();
                        GEOMETRIX_ASSERT(!succ || succ->get_top_level() <= max_level::value);
                        if (mark)
                        {
                            pred->next(level).set(succ, 0);
                            curr->unlink();
                        }
                        curr = succ;
                    } while (mark && curr && curr->get_top_level() >= level);
                }
            }
            pred = pred->next(0).get_ptr();
        }
    }

    bool is_unlinked_full_scan(node_ptr pVictim)
    {
        mark_type m,n;
        node_ptr curr, succ;
        auto pred = m_pHead;
        while (pred)
        {
            if (pred == pVictim)
                return false;
            auto predTop = pred->get_top_level();
            for(int level = 0UL; level <= predTop; ++level)
            {
                std::tie(curr, m) = pred->next(level).get();
                std::tie(succ, n) = curr ? curr->next(level).get() : std::make_pair(curr, false);
                if (curr == pVictim)
                    return false;
            }
            std::tie(pred, m) = pred->next(0).get();
        }

        return true;
    }

    bool is_unlinked(node_ptr pVictim)
    {
        auto x = resolve_key(pVictim->item());
        int bottomLevel = 0;
        mark_type mark = 0;
        node_ptr pred = m_pHead;
        node_ptr curr = nullptr;
        node_ptr succ = nullptr;
        for (int level = max_level::value; level >= bottomLevel; --level)
        {
            curr = pred->next(level).get_ptr();
            while (curr)
            {
                std::tie(succ, mark) = curr->next(level).get();
                while (mark)
                {
                    if (curr == pVictim)
                        return false;
                    curr = curr->next(level).get_ptr();
                    if (curr)
                        std::tie(succ, mark) = curr->next(level).get();
                    else
                        break;
                }
                if (curr && less(curr, x))
                {
                    pred = curr;
                    curr = succ;
                }
                else
                    break;
            }
        }

        return true;
    }

    template <typename Value>
    std::pair<iterator, bool> add( Value&& x )
    {
        return add_or_update(std::forward<Value>(x), [](bool, value_type&) {});
    }

    template <typename Value, typename Fn>
    std::pair<iterator, bool> add_or_update(Value&& x, Fn&& updateFn)
    {
        auto pHead = m_pHead;
        int topLevel = m_selector(pHead->get_top_level());
        int bottomLevel = 0;
        std::array<node_ptr, max_height::value> preds;
        std::array<node_ptr, max_height::value> succs;
        std::size_t newSize;
        node_ptr pNewNode;
        auto key = resolve_key(x);
        while( true )
        {
            if(find(key, preds, succs))
            {
                iterator it(succs[bottomLevel]);
                updateFn(false, *it);
                return std::make_pair<iterator,bool>(std::move(it), false);
            }
            else
            {
                pNewNode = create_node( std::forward<Value>(x), topLevel );
                for(auto level = bottomLevel; level <= topLevel; ++level)
                {
                    auto succ = succs[level];
                    pNewNode->set_next(level, succ, 0);
                }

                auto pred = preds[bottomLevel];
                auto succ = succs[bottomLevel];
                pNewNode->set_next(bottomLevel, succ, 0);
                auto expectedStamp = mark_type{ 0 };

                if( !pred->next(bottomLevel).compare_exchange_strong(succ, expectedStamp, pNewNode, 0) )
                {
                    delete_node(pNewNode);
                    continue;
                }

                updateFn(true, pNewNode->item());

                for(int level = bottomLevel+1; level <= topLevel; ++level)
                {
                    while (true)
                    {
                        pred = preds[level];
                        succ = succs[level];
                        auto sExpected = mark_type{ 0 };
                        if (pred->next(level).compare_exchange_strong(succ, sExpected, pNewNode, 0))
                            break;
                        find(key, preds, succs);
                    }
                }

                newSize = increment_size();
                return std::make_pair(iterator(pNewNode), true);
            }
        }
    }

    size_type increment_size() { return m_size.fetch_add(1, std::memory_order_relaxed) + 1; }
    size_type decrement_size() { return m_size.fetch_sub(1, std::memory_order_relaxed) - 1; }

    node_ptr m_pHead{ nullptr };
    level_selector m_selector;
    std::atomic<size_type> m_size{ 0 };
};

//! Declare a set type.
template <typename Key, typename Compare = std::less< Key >, typename Alloc = std::allocator< Key > >
class concurrent_set : public lock_free_concurrent_skip_list< detail::associative_set_traits< Key, Compare, Alloc, 32, false > >
{
    using base_type = lock_free_concurrent_skip_list< detail::associative_set_traits< Key, Compare, Alloc, 32, false > >;
    using max_level = typename base_type::max_level;

public:

    concurrent_set( const Compare& c = Compare() )
        : base_type( max_level::value, c )
    {}
};

//! Declare a map type.
template <typename Key, typename Value, typename Compare = std::less< Key >, typename Alloc = std::allocator< Key > >
class concurrent_map : public lock_free_concurrent_skip_list< detail::associative_map_traits< Key, Value, Compare, Alloc, 32, false > >
{
    using base_type = lock_free_concurrent_skip_list< detail::associative_map_traits< Key, Value, Compare, Alloc, 32, false > >;
    using max_level = typename base_type::max_level;

public:

    typedef Value                                              mapped_type;
    typedef Key                                                key_type;
    typedef typename boost::add_reference< mapped_type >::type reference;
    typedef typename boost::add_const< mapped_type >::type     const_reference;
    using iterator = typename base_type::iterator;

    concurrent_map( const Compare& c = Compare() )
        : base_type( max_level::value, c )
    {}

    //! This interface admits concurrent reads to find a default constructed value for a given key if there is a writer using this.
    reference operator [] ( const key_type& k )
    {
        return this->add_or_update(std::make_pair(k, mapped_type()), [](bool, std::pair<const key_type, mapped_type>&) {}).first->second;
    }

    template <typename UpdateFn>
    std::pair<iterator, bool> insert_or_update(const key_type& key, UpdateFn&& fn)
    {
        return this->add_or_update(std::make_pair(key, mapped_type()), std::forward<UpdateFn>(fn));
    }

};

//! Declare a map type with a variable height parameter.
template <typename Key, typename Value, unsigned int MaxHeight, typename Compare = std::less< Key >, typename Alloc = std::allocator< Key > >
class concurrent_skip_map : public lock_free_concurrent_skip_list< detail::associative_map_traits< Key, Value, Compare, Alloc, MaxHeight, false > >
{
    using base_type = lock_free_concurrent_skip_list< detail::associative_map_traits< Key, Value, Compare, Alloc, MaxHeight, false > >;

public:

    typedef Value                                              mapped_type;
    typedef Key                                                key_type;
    typedef typename boost::add_reference< mapped_type >::type reference;
    typedef typename boost::add_const< mapped_type >::type     const_reference;
    using iterator = typename base_type::iterator;

    concurrent_skip_map( const Compare& c = Compare() )
        : base_type( MaxHeight - 1, c )
    {}

    //! This interface admits concurrent reads to find a default constructed value for a given key if there is a writer using this.
    reference operator [] ( const key_type& k )
    {
        return this->add_or_update(std::make_pair(k, mapped_type()), [](bool, std::pair<const key_type, mapped_type>&) {}).first->second;
    }

    template <typename UpdateFn>
    std::pair<iterator, bool> insert_or_update(const key_type& key, UpdateFn&& fn)
    {
        return this->add_or_update(std::make_pair(key, mapped_type()), std::forward<UpdateFn>(fn));
    }

};

}//! namespace stk;

#endif//! STK_CONTAINER_LOCK_FREE_CONCURRENT_SKIP_LIST_HPP


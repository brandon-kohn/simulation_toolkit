//
//! Copyright © 2013-2017
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

//! Multiple improvements were included from studying folly's ConcurrentSkipList:
/*
* Copyright 2017 Facebook, Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
// @author: Xin Liu <xliux@fb.com>
//

#ifndef STK_CONTAINER_CONCURRENT_SKIP_LIST_HPP
#define STK_CONTAINER_CONCURRENT_SKIP_LIST_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include <geometrix/utility/assert.hpp>

#include <boost/iterator.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/mpl/int.hpp>
#include <stk/thread/tiny_atomic_spin_lock.hpp>
#include <stk/utility/scope_exit.hpp>

#include <array>
#include <random>
#include <list>
#include <mutex>
#include <type_traits>
#include <memory>

//! A concurrent skip list implementation with map and set versions.

#define STK_SKIP_LIST_MAX_HEIGHT 64

namespace stk { 
    namespace detail {

    //! Associative map traits to make the lazy list behave like a map<key, value>
    template < typename Key,             //! key type
        typename Value,                  //! mapped type
        typename Pred,                   //! comparator predicate type
        typename Alloc,                  //! actual allocator type (should be value allocator)
        unsigned int MaxHeight,           //! Max size of the skip list node array.
        bool allowMultipleKeys = false,  //! true if multiple equivalent keys are permitted
		typename Mutex = stk::thread::tiny_atomic_spin_lock<>>
    struct associative_map_traits
    {
        typedef Key                                                key_type;
        typedef std::pair<const Key, Value>                        value_type;
        typedef Pred                                               key_compare;
        typedef typename Alloc::template rebind<value_type>::other allocator_type;
        typedef boost::mpl::int_<MaxHeight>                        max_height;
        typedef boost::mpl::int_<MaxHeight-1>                      max_level;
        typedef boost::mpl::bool_<allowMultipleKeys>               allow_multiple_keys;
		typedef Mutex                                              mutex_type;
		using size_type = std::size_t;

        associative_map_traits(Pred _Parg, Alloc _Al)
            : m_compare(_Parg)
        {}

        class value_compare
        {
            friend struct associative_map_traits<Key, Value, Pred, Alloc, MaxHeight, allowMultipleKeys, Mutex>;

        public:

            using result_type = bool;

            bool operator()(const value_type& _Left, const value_type& _Right) const
            {
                return (m_compare(_Left.first, _Right.first));
            }

            value_compare(key_compare _Pred)
                : m_compare(_Pred)
            {}

        protected:
            key_compare m_compare;  // the comparator predicate for keys
        };

        BOOST_FORCEINLINE static const Key& resolve_key( const value_type& v )
        {
            return (v.first);
        }
		

        Pred m_compare; // the comparator predicate for keys
    };

    //! Associative map traits to make the lazy list behave like a map<key, value>
    template < typename Key,             //! key type
        typename Pred,                   //! comparator predicate type
        typename Alloc,                  //! actual allocator type (should be value allocator)
        unsigned int MaxHeight,          //! Max size of the skip list node array.		
        bool AllowMultipleKeys = false,  //! true if multiple equivalent keys are permitted
		typename Mutex = std::mutex> 
    struct associative_set_traits
    {
        typedef Key                                                key_type;
        typedef Key                                                value_type;
        typedef Pred                                               key_compare;
        typedef typename Alloc::template rebind<value_type>::other allocator_type;
		typedef Mutex                                              mutex_type;
        typedef boost::mpl::bool_<AllowMultipleKeys>               allow_multiple_keys;
        typedef boost::mpl::int_<MaxHeight>                        max_height;
        typedef boost::mpl::int_<MaxHeight-1>                      max_level;
		using size_type = std::size_t;

        associative_set_traits(Pred p, Alloc /*al*/)
            : m_compare(p)
        {}

        typedef key_compare value_compare;
        BOOST_FORCEINLINE static const Key& resolve_key(const value_type& v)
        {
            return v;
        }
   

        Pred m_compare; // the comparator predicate for keys
    };

    template<typename AssociativeTraits>
    class skip_list_node_manager : public AssociativeTraits
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

        struct node 
        {
            enum flag : std::uint8_t
            {
                Head = 1
			  , MarkedForRemoval = (1 << 1)
              , FullyLinked = (1 << 2)
            };

            using node_levels = std::atomic<node_ptr>*;

			std::uint8_t     get_flags() const { return flags.load(std::memory_order_consume); }
			void			 set_flags(std::uint8_t f) { flags.store(f, std::memory_order_release); }
			bool			 is_head() const { return get_flags() & flag::Head; }
			bool			 is_fully_linked() const { return get_flags() & flag::FullyLinked; }
			bool			 is_marked_for_removal() const { return get_flags() & flag::MarkedForRemoval; }
			void			 set_is_head() { set_flags(std::uint8_t(get_flags() | flag::Head)); }
			void			 set_marked_for_removal() { set_flags(std::uint8_t(get_flags() | flag::MarkedForRemoval)); }
			void			 set_fully_linked() { set_flags(std::uint8_t(get_flags() | flag::FullyLinked)); }
            const key_type&  key() const { return traits::resolve_key( value_ ); }
            value_type&      item() { return value_; }
            node_levels&     next() { return nexts; }
			node_ptr         next(std::uint8_t i) const { GEOMETRIX_ASSERT(i <= topLevel); return nexts[i].load(std::memory_order_consume); }
			void             set_next(std::uint8_t i, node_ptr pNode) { GEOMETRIX_ASSERT(i <= topLevel); return nexts[i].store(pNode, std::memory_order_release); }
			std::uint8_t	 get_top_level() const { return topLevel; }
			mutex_type&      get_mutex() { return mutex; }

		private:

			friend class skip_list_node_manager<traits>;

			template <typename Value>
			node(std::uint8_t topLevel, Value&& data, bool isHead)
				: value_(std::forward<Value>(data))
				, flags(!isHead ? 0 : 0 | flag::Head)
				, topLevel(topLevel)
			{
				GEOMETRIX_ASSERT(isHead == is_head());
				GEOMETRIX_ASSERT(!is_marked_for_removal());
				GEOMETRIX_ASSERT(!is_fully_linked());
				for (std::uint8_t i = 0; i <= topLevel; ++i)
					new(&nexts[i]) std::atomic<node*>(nullptr);
			}

            value_type			      value_;
            std::atomic<std::uint8_t> flags;
			mutex_type                mutex;
			std::uint8_t              topLevel;
            std::atomic<node*>        nexts[0];//! C trick for tight dynamic arrays.
        };

        skip_list_node_manager( key_compare p, allocator_type al )
            : AssociativeTraits( p, al )
            , m_scopeManager( std::make_shared<node_scope_manager>(al) )
        {}
		
		template <typename Data>
		node_ptr create_node( Data&& v, std::uint8_t topLevel, bool isHead = false)
		{
			return m_scopeManager->create_node(v, topLevel, isHead);
		}

		void clone_head_node(node_ptr pHead, node_ptr pNew)
		{
			GEOMETRIX_ASSERT(pHead);
			pNew->set_flags(pHead->get_flags());
			for (std::size_t i = 0; i <= pHead->get_top_level(); ++i)
				pNew->set_next(i, pHead->next(i));
		}

		void destroy_node( node_ptr pNode )
		{
			m_scopeManager->destroy_node(pNode);
		}

		void really_destroy_node( node_ptr pNode )
		{
			m_scopeManager->really_destroy_node(pNode);
		}

		key_compare key_comp() const { return m_compare; }
		bool less(node_ptr pNode, const key_type& k) const
		{
			return pNode->is_head() || key_comp()(pNode->key(), k);
		}

		bool equal(node_ptr pNode, const key_type& k) const
		{
			return !pNode->is_head() && !key_comp()(pNode->key(), k) && !key_comp()(k, pNode->key());
		}

		class node_scope_manager;
		std::shared_ptr<node_scope_manager> get_scope_manager() const { return m_scopeManager; }

	private:    

		class node_scope_manager
		{
		public:
			node_scope_manager(allocator_type al)
				: m_nodeAllocator(al)
			{}

			~node_scope_manager()
			{
				//! There should not be any iterators checked out at this point.
				GEOMETRIX_ASSERT(m_refCounter.load(std::memory_order_relaxed) == 0);
				if (m_nodes)
					for (auto pNode : *m_nodes)
						really_destroy_node(pNode);
			}

			template <typename Data>
			node_ptr create_node(Data&& v, std::uint8_t topLevel, bool isHead = false)
			{
				std::size_t aSize = sizeof(node) + (topLevel + 1) * sizeof(std::atomic<node_ptr>);
				auto pNode = reinterpret_cast<node_ptr>(m_nodeAllocator.allocate(aSize));
				new(pNode) node(topLevel, std::forward<Data>(v), isHead);
				return pNode;
			}

			void destroy_node(node_ptr pNode)
			{
				auto lk = std::unique_lock<mutex_type>{ m_mtx };
				if (m_nodes)
					m_nodes->push_back(pNode);
				else
					m_nodes = std::make_unique<std::vector<node_ptr>>(1, pNode);

				m_hasNodes.store(true, std::memory_order_relaxed);
			}

			void add_checkout()
			{
				m_refCounter.fetch_add(1, std::memory_order_relaxed);
			}

			void remove_checkout()
			{
				STK_MEMBER_SCOPE_EXIT
				(
					GEOMETRIX_ASSERT(m_refCounter > 0);
				    m_refCounter.fetch_sub(1, std::memory_order_relaxed);
				);

				if (m_hasNodes.load(std::memory_order_relaxed) && m_refCounter.load(std::memory_order_relaxed) < 1)
				{
					std::unique_ptr<std::vector<node_ptr>> toDelete;
					{
						auto lk = std::unique_lock<mutex_type>{ m_mtx };
						if (m_nodes.get() && m_refCounter.load(std::memory_order_relaxed) < 1)
						{
							toDelete.swap(m_nodes);
							m_hasNodes.store(false, std::memory_order_relaxed);
						}
					}
					for (auto pNode : *toDelete)
						really_destroy_node(pNode);
				}
			}

			void really_destroy_node(node_ptr pNode)
			{
				pNode->~node();
				std::size_t aSize = sizeof(node) + (pNode->get_top_level() + 1) * sizeof(std::atomic<node_ptr>);
				m_nodeAllocator.deallocate((std::uint8_t*)pNode, aSize);
			}

		private:

			using node_allocator = typename allocator_type::template rebind<std::uint8_t>::other;   // allocator object for nodes is a byte allocator.
			node_allocator m_nodeAllocator; 
			std::atomic<std::uint32_t> m_refCounter{ 0 };
			std::unique_ptr<std::vector<node_ptr>> m_nodes;
			std::atomic<bool> m_hasNodes{ false };
			mutex_type m_mtx;
		};

		std::shared_ptr<node_scope_manager> m_scopeManager;
    };

    class random_xor_shift_generator
    {   
    public:

		using result_type = unsigned int;
		static result_type min BOOST_PREVENT_MACRO_SUBSTITUTION() { return 0; }
		static result_type max BOOST_PREVENT_MACRO_SUBSTITUTION() { return (std::numeric_limits<result_type>::max)(); }
            
        random_xor_shift_generator(unsigned int seed = 42)
        {
            m_state.store(seed, std::memory_order_relaxed);
        }

		random_xor_shift_generator(const random_xor_shift_generator&rhs)
			: m_state(rhs.m_state.load())
		{

		}

		random_xor_shift_generator& operator=(const random_xor_shift_generator&rhs)
		{
			m_state.store(rhs.m_state.load());
		}

        BOOST_FORCEINLINE unsigned int operator()()
        {
            auto x = m_state.load(std::memory_order_relaxed);
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            m_state.store(x, std::memory_order_relaxed);
            return x;
        }
    private:

        std::atomic<unsigned int> m_state;

    };

	template <std::uint8_t MaxHeight>
	struct size_table : std::array<std::uint64_t, MaxHeight>
	{
		size_table()
		{
			for (int i = 0; i < MaxHeight; ++i)
				(*this)[i] = (std::min)(std::uint64_t(1ULL << i), (std::numeric_limits<std::uint64_t>::max)());
		}
	};

	static size_table<STK_SKIP_LIST_MAX_HEIGHT> const& get_size_table()
	{
		static size_table<STK_SKIP_LIST_MAX_HEIGHT> instance;
		return instance;
	}
}//! namespace detail;


template <std::uint8_t MaxHeight>
struct skip_list_level_selector
{
	using max_level = std::integral_constant<std::uint8_t, MaxHeight - 1>;

	struct probability_table
	{
		probability_table(double p = 0.5)
		{
			probabilities[0] = 1.0;
			for (int i = 1; i < MaxHeight; ++i)
				probabilities[i] = std::pow(p, i);
		}

		std::array<double, MaxHeight>        probabilities;
	};

	static probability_table const& get_probs()
	{
		static probability_table instance;
		return instance;
	}

	static detail::random_xor_shift_generator create_generator()
	{
		std::random_device rd;
		return detail::random_xor_shift_generator(rd());
	}

	static double rnd()
	{
		static detail::random_xor_shift_generator eng = create_generator();
		static std::uniform_real_distribution<> dist;
		return dist(eng);
	}

	std::uint8_t operator()(std::uint8_t maxLevel = max_level::value) const
	{
		auto lvl = std::uint8_t{ 0 };
		auto const& probs = get_probs().probabilities;
		while (rnd() < probs[lvl + 1] && lvl < maxLevel)
			++lvl;
		return lvl;
	}
};

template <std::uint8_t MaxHeight>
struct coin_flip_level_selector
{
	using max_level = std::integral_constant<std::uint8_t, MaxHeight - 1>;

	static detail::random_xor_shift_generator create_generator()
	{
		std::random_device rd;
		return random_xor_shift_generator(rd());
	}

	static std::uint64_t rnd()
	{
		static detail::random_xor_shift_generator eng = create_generator();
		std::uniform_int_distribution<std::uint64_t> dist;
		return dist(eng);
	}

	std::uint8_t operator()(std::uint8_t maxLevel = max_level::value) const
	{
		auto x = rnd();
		if ((x & 1) != 0)
			return 0;

		auto lvl = std::uint8_t{ 1 };
		while ((((x >>= 1) & 1) != 0) && lvl < maxLevel)
			++lvl;
		return lvl;
	}
};

template <typename AssociativeTraits, typename LevelSelectionPolicy = skip_list_level_selector<AssociativeTraits::max_level::value+1>>
class lazy_concurrent_skip_list : public detail::skip_list_node_manager<AssociativeTraits>
{
	static_assert(AssociativeTraits::max_height::value > 1 && AssociativeTraits::max_height::value <= 64, "MaxHeight should be in the range [2, 64]");
public:
	using traits_type = AssociativeTraits;
	using size_type = typename traits_type::size_type;
    using key_type = typename traits_type::key_type;
    using value_type = typename traits_type::value_type;
    using key_compare = typedef typename traits_type::key_compare;
    using allocator_type = typename traits_type::allocator_type;
    using reference = typename std::add_lvalue_reference<value_type>::type;
    using const_reference = typename std::add_const<reference>::type;
    using level_selector = LevelSelectionPolicy;

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

        friend class lazy_concurrent_skip_list< traits_type, level_selector >;

        node_iterator()
            : m_pNode()
        {}

		template <typename U>
		node_iterator(const node_iterator<U>& other)
			: m_pNode(other.m_pNode)
			, m_pNodeManager(other.m_pNodeManager)
		{
			if (m_pNode)
				aquire();
		}

		node_iterator& operator = (const node_iterator& rhs)
		{
			if (m_pNode && !rhs.m_pNode)
				release();
			m_pNodeManager = rhs.m_pNodeManager;
			if (!m_pNode && rhs.m_pNode)
				aquire();
			m_pNode = rhs.m_pNode;
			return *this;
		}

		node_iterator(node_iterator&& other)
			: m_pNode(other.m_pNode)
			, m_pNodeManager(std::move(other.m_pNodeManager))
		{
			other.m_pNode = nullptr;
		}
		
		node_iterator& operator = (node_iterator&& rhs)
		{
			if (m_pNode)
				release(); 
			m_pNodeManager = std::move(rhs.m_pNodeManager);		
			m_pNode = rhs.m_pNode;
			rhs.m_pNode = nullptr;
			return *this;
		}

        explicit node_iterator( const std::shared_ptr<node_scope_manager>& pNodeManager, node_ptr pNode )
            : m_pNode( pNode )
			, m_pNodeManager(pNodeManager)
        {
			if (m_pNode)
				pNodeManager->add_checkout();
		}

		//! Aquire the underlying lock of the node and execute v(*iter).
		template <typename Visitor>
		void exec_under_lock(Visitor&& v)
		{
			GEOMETRIX_ASSERT(m_pNode);
			auto lk = std::unique_lock<mutex_type>{ m_pNode->get_mutex() };
			v(dereference());
		}

		~node_iterator()
		{
			if (m_pNode)
				release();
		}

    private:

		template <typename T>
		BOOST_FORCEINLINE bool is_uninitialized(std::weak_ptr<T> const& weak) 
		{
			using wt = std::weak_ptr<T>;
			return !weak.owner_before(wt{}) && !wt{}.owner_before(weak);
		}

        friend class boost::iterator_core_access;

		void release()
		{
			GEOMETRIX_ASSERT(is_uninitialized(m_pNodeManager) || !m_pNodeManager.expired());
			auto pMgr = m_pNodeManager.lock();
			if (pMgr)
				pMgr->remove_checkout();
		}
		
		void aquire()
		{
			GEOMETRIX_ASSERT(is_uninitialized(m_pNodeManager) || !m_pNodeManager.expired());
			auto pMgr = m_pNodeManager.lock();
			if (pMgr)
				pMgr->add_checkout();
		}

        void increment()
        {
			if (m_pNode)
			{
				m_pNode = m_pNode->next(0);
				if (!m_pNode)
					release();
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
		std::weak_ptr<node_scope_manager> m_pNodeManager;
    };

    struct const_iterator;

    struct iterator : node_iterator< value_type >
    {
        iterator()
            : node_iterator< value_type >()
        {}

        iterator( const std::shared_ptr<node_scope_manager>& pMgr, node_ptr pNode )
            : node_iterator< value_type >( pMgr, pNode )
        {}

		template <typename U>
        bool operator ==( node_iterator<U> const& other ) const
        {
            return m_pNode == other.m_pNode;
        }

		template <typename U>
        bool operator !=( node_iterator<U> const& other ) const
        {
            return m_pNode != other.m_pNode;
        }
    };

    struct const_iterator : node_iterator< const value_type >
    {
        const_iterator()
            : node_iterator< const value_type >()
        {}

        const_iterator(const std::shared_ptr<node_scope_manager>& pMgr, node_ptr pNode )
            : node_iterator< const value_type >(pMgr, pNode)
        {}

        const_iterator( iterator const& other )
            : node_iterator< const value_type >( other )
        {}

        const_iterator operator =( iterator const& other )
        {
            m_pNode = other.m_pNode;
            return *this;
        }

        bool operator ==( iterator const& other ) const
        {
            return m_pNode == other.m_pNode;
        }

        bool operator ==( const_iterator const& other ) const
        {
            return m_pNode == other.m_pNode;
        }

        bool operator !=( iterator const& other ) const
        {
            return m_pNode != other.m_pNode;
        }

        bool operator !=( const_iterator const& other ) const
        {
            return m_pNode != other.m_pNode;
        }
    };

    lazy_concurrent_skip_list( std::uint8_t topLevel = 1, const key_compare& pred = key_compare(), const allocator_type& al = allocator_type() )
        : detail::skip_list_node_manager<AssociativeTraits>( pred, al )
        , m_selector()
        , m_pHead(create_node(value_type(), topLevel, true))
    {
        GEOMETRIX_ASSERT( m_pHead );
    }

    ~lazy_concurrent_skip_list()
    {
        release();
    }

    iterator begin()
    {
        return iterator(get_scope_manager(), left_most());
    }

    const_iterator begin() const
    {
        return const_iterator(get_scope_manager(), left_most());
    }

    iterator end()
    {
        return iterator(std::shared_ptr<node_scope_manager>(), nullptr);
    }

    const_iterator end() const
    {
        return const_iterator(std::shared_ptr<node_scope_manager>(), nullptr);
    }

    iterator find( const key_type& x )
    {
        std::array<node_ptr, max_height::value> preds;
        std::array<node_ptr, max_height::value> succs;

        int lFound = find( x, preds, succs );
        if( lFound != -1 )
        {
            node_ptr pFound = succs[lFound];
            GEOMETRIX_ASSERT( pFound );
            if( pFound && pFound->is_fully_linked() && !pFound->is_marked_for_removal() )
                return iterator(get_scope_manager(), pFound);
        }

        return end();
    }

    const_iterator find( const key_type& x ) const
    {
        std::array<node_ptr, max_height::value> preds;
        std::array<node_ptr, max_height::value> succs;

        int lFound = find( x, preds, succs );
        if( lFound != -1 )
        {
            auto pFound = succs[lFound];
            GEOMETRIX_ASSERT( pFound );
            if( pFound && pFound->is_fully_linked() && !pFound->is_marked_for_removal() )
                return const_iterator(get_scope_manager(),pFound);
        }

        return end();
    }

    iterator lower_bound( const key_type& x )
    {
        std::array<node_ptr, max_height::value> preds;
        std::array<node_ptr, max_height::value> succs;

        int lFound = lower_bound( x, preds, succs );
        if( lFound != -1 )
        {
            auto pFound = succs[lFound];
            GEOMETRIX_ASSERT( pFound );
            if (pFound && pFound->is_fully_linked() && !pFound->is_marked_for_removal())
                return iterator(get_scope_manager(), pFound);
        }

        return end();
    }

    const_iterator lower_bound( const key_type& x ) const
    {
        std::array<node_ptr, max_height::value> preds;
        std::array<node_ptr, max_height::value> succs;

        int lFound = lower_bound( x, preds, succs );
        if( lFound != -1 )
        {
            auto pFound = succs[lFound];
            GEOMETRIX_ASSERT( pFound );
            if (pFound && pFound->is_fully_linked() && !pFound->is_marked_for_removal())
                return const_iterator(get_scope_manager(), pFound);
        }

        return end();
    }

    std::pair< iterator, bool > insert( const value_type& item )
    {
        return add( item );
    }

    iterator insert( const iterator&, const value_type& item )
    {
        return add( item ).first;
    }

    bool contains( const key_type& x ) const
    {
        std::array<node_ptr, max_height::value> preds;
        std::array<node_ptr, max_height::value> succs;

        int lFound = find( x, preds, succs );
        if( lFound != -1 )
        {
            auto pFound = succs[lFound];
            return (pFound && pFound->is_fully_linked() && !pFound->is_marked_for_removal());
        }

        return false;
    }

    iterator erase(const key_type& x)
    {
        using lock_array = std::array<lock_type, max_height::value>;
        node_ptr pVictim;
		lock_type victimLock;
        bool isMarked = false;
        int topLevel = -1;
        std::array<node_ptr, max_height::value> preds;
        std::array<node_ptr, max_height::value> succs;

        while (true)
        {
            int lFound = find(x, preds, succs);
            if (lFound != -1)
            {
                pVictim = succs[lFound];
                GEOMETRIX_ASSERT(pVictim);
            }

            if (
                isMarked ||
                (
                    lFound != -1 &&
                    (pVictim->is_fully_linked() && pVictim->get_top_level() == lFound && !pVictim->is_marked_for_removal())
                )
               )
            {
                if (!isMarked)
                {
                    if (pVictim->is_marked_for_removal())
                        return end();
                    
                    topLevel = pVictim->get_top_level();
					victimLock = lock_type{pVictim->get_mutex()};
                    pVictim->set_marked_for_removal();
                    isMarked = true;
                }

                node_ptr pPred, pPrevPred = nullptr;
                bool valid = true;
				lock_array locks;
                for (int level = 0; valid && level <= topLevel; ++level)
                {
                    pPred = preds[level];

                    GEOMETRIX_ASSERT(pPred);
                    if (!pPred)
                        return end();

					if (pPrevPred != pPred)
					{
						locks[level] = lock_type{ pPred->get_mutex() };
						pPrevPred = pPred;
					}
                    valid = !pPred->is_marked_for_removal() && pPred->next(level) == pVictim;
                }

                if (!valid)
                    continue;

				for (int level = topLevel; level >= 0; --level)
					preds[level]->set_next(level, pVictim->next(level));

				decrement_size();

                //! Save return iterator.
                iterator nextIT(get_scope_manager(), pVictim->next(0));

                victimLock.unlock();

                destroy_node( pVictim );

                return nextIT;
            }
			else
                return end();
        }
    }

    iterator erase(const_iterator it)
    {
        if (it == end())
            return end();

        const value_type& x = *it;
        return erase(resolve_key(x));
    }

	//! Clear is thread-safe in the strict sense of not invalidating iterators, but the results are 
	//! not well defined in the presence of other writers.
	//! If true atomic clear semantics are required, consider holding an atomic ptr
	//! to the map instance and swapping that when required.
	//! TODO: This is not optimal.
    void clear()
    {
		for (auto it = begin(); it != end(); ++it)
			erase(it);
    }
	
	//! Returns the size at any given moment. This can be volatile in the presence of writers in other threads.
	size_type size() const { return m_size.load(std::memory_order_relaxed); }
	
	//! Returns empty state at any given moment. This can be volatile in the presence of writers in other threads.
	bool empty() const { return size() == 0; }

protected:

    using lock_type = std::unique_lock<mutex_type>;
 
    node_ptr left_most() const
    {
        return m_pHead.load(std::memory_order_consume)->next(0);
    }

    void release()
    {
        GEOMETRIX_ASSERT( m_pHead );

		auto pCurr = m_pHead.load(std::memory_order_relaxed);
		while (pCurr)
		{
			auto pNext = pCurr->next(0);
			really_destroy_node(pCurr);
			pCurr = pNext;
		}
    }

	template <typename Preds, typename Succs>
	int find(const key_type& key, Preds& preds, Succs& succs)
	{
		auto pHead = m_pHead.load(std::memory_order_consume);
		int topLevel = pHead->get_top_level();
		return find(key, preds, succs, pHead, topLevel);
	}

	template <typename Preds, typename Succs>
    int find( const key_type& key, Preds& preds, Succs& succs, node_ptr pHead, std::uint8_t topLevel ) const
    {
        int lFound = -1;
        node_ptr pPred = pHead;
        node_ptr pFound = nullptr;
		GEOMETRIX_ASSERT(pPred);
        for( int level = topLevel; level >= 0; --level )
        {
            node_ptr pCurr = pPred->next(level);
            while( pCurr && less(pCurr, key ))//key > curr.key
            {
                pPred = pCurr;
                pCurr = pPred->next(level);
            }

			if (pCurr && lFound == -1 && equal(pCurr, key))
			{
				lFound = level;
				pFound = pCurr;
			}

            GEOMETRIX_ASSERT( static_cast<int>( preds.size() ) >= level );
            GEOMETRIX_ASSERT( static_cast<int>( succs.size() ) >= level );
            //if( static_cast<int>( preds.size() ) >= level )
            preds[level] = pPred;
            //if( static_cast<int>( succs.size() ) >= level )
            succs[level] = !pFound ? pCurr : pFound;
        }

        return lFound;
    }

    template <typename Preds, typename Succs>
    int lower_bound( const key_type& key, Preds& preds, Succs& succs ) const
    {
        int lFound = -1;
        node_ptr pPred = m_pHead.load(std::memory_order_consume);
        for( int level = max_level(); level >= 0; --level )
        {
            node_ptr pCurr = pPred->next(level);
            GEOMETRIX_ASSERT( pCurr );
            while( pCurr && less(pCurr, key)) //key > curr.key
            {
                pPred = pCurr;
                pCurr = pPred->next(level);
            }

            if( pCurr && !less(key, pCurr))
                lFound = level;

            GEOMETRIX_ASSERT( static_cast<int>( preds.size() ) >= level );
            GEOMETRIX_ASSERT( static_cast<int>( succs.size() ) >= level );
            //if( static_cast<int>( preds.size() ) >= level )
            preds[level] = pPred;
            //if( static_cast<int>( succs.size() ) >= level )
            succs[level] = pCurr;
        }

        return lFound;
    }

	template <typename Value>
    std::pair< iterator, bool > add( Value&& x )
    {
        //int topLevel = m_selector();
        std::array<node_ptr, max_height::value> preds;
        std::array<node_ptr, max_height::value> succs;
        using lock_array = std::array<lock_type, max_height::value>;
		std::size_t newSize;
		node_ptr pNewNode;
        while( true )
        {
			auto pHead = m_pHead.load(std::memory_order_consume);
			int topLevel = pHead->get_top_level();
            int lFound = find( resolve_key(x), preds, succs, pHead, topLevel );
            if( lFound != -1 )
            {
                node_ptr pFound = succs[lFound];
                GEOMETRIX_ASSERT( pFound );
                if( !pFound )
                    return std::make_pair( end(), false );

                if( !pFound->is_marked_for_removal() )
                {
                    while( !pFound->is_fully_linked() ){}
                    return std::make_pair( iterator(get_scope_manager(), pFound), false );
                }

                continue;
            }

			topLevel = m_selector(topLevel);
            node_ptr pPred;
            node_ptr pPrevPred = nullptr;
            node_ptr pSucc;
            bool valid = true;
			lock_array locks;
            for( int level = 0; valid && level <= topLevel; ++level )
            {
                pPred = preds[level];
                pSucc = succs[level];

				if (pPrevPred != pPred)
				{
					locks[level] = lock_type{ pPred->get_mutex() };
					pPrevPred = pPred;
				}
                valid = !pPred->is_marked_for_removal() && (!pSucc || !pSucc->is_marked_for_removal()) && pPred->next(level) == pSucc;
            }

            if( !valid )
                continue;

            pNewNode = create_node( std::forward<Value>(x), topLevel );
            
			for( int level = 0; level <= topLevel; ++level )
                pNewNode->set_next(level,succs[level]);
            for( int level = 0; level <= topLevel; ++level )
                preds[level]->set_next(level,pNewNode);

            pNewNode->set_fully_linked();
			newSize = increment_size();
			break;
        }

		auto tLvl = m_pHead.load(std::memory_order_relaxed)->get_top_level();
		if ((tLvl + 1) < max_height::value && newSize > detail::get_size_table()[tLvl])
			increment_height(tLvl + 1);

		return std::make_pair(iterator(get_scope_manager(), pNewNode), true);
    }

	size_type increment_size() { return m_size.fetch_add(1, std::memory_order_relaxed) + 1; }
	size_type decrement_size() { return m_size.fetch_sub(1, std::memory_order_relaxed) - 1; }
	void increment_height(std::uint8_t tLvl)
	{
		auto pHead = m_pHead.load(std::memory_order_consume);
		if (pHead->get_top_level() >= tLvl)
			return;
		GEOMETRIX_ASSERT(tLvl > pHead->get_top_level());
		auto pNew = create_node(value_type(), tLvl, true);
		{
			auto lk = std::unique_lock<mutex_type>{ pHead->get_mutex() };
			clone_head_node(pHead, pNew);
			auto expected = pHead;
			if (!m_pHead.compare_exchange_strong(expected, pNew, std::memory_order_release))
			{
				really_destroy_node(pNew);
				return;
			}
			pHead->set_marked_for_removal();
		}
		destroy_node(pHead);
	}

    std::atomic<node_ptr> m_pHead;
    level_selector m_selector;
	std::atomic<size_type> m_size{ 0 };
};

//! Declare a set type.
template <typename Key, typename Compare = std::less< Key >, typename Alloc = std::allocator< Key > >
class concurrent_set : public lazy_concurrent_skip_list< detail::associative_set_traits< Key, Compare, Alloc, 32, false > >
{
    typedef lazy_concurrent_skip_list< detail::associative_set_traits< Key, Compare, Alloc, 32, false > > BaseType;

public:

    concurrent_set( const Compare& c = Compare() )
        : BaseType( 1, c )
    {}
};

//! Declare a map type.
template <typename Key, typename Value, typename Compare = std::less< Key >, typename Alloc = std::allocator< Key > >
class concurrent_map : public lazy_concurrent_skip_list< detail::associative_map_traits< Key, Value, Compare, Alloc, 32, false > >
{
    typedef lazy_concurrent_skip_list< detail::associative_map_traits< Key, Value, Compare, Alloc, 32, false > > BaseType;

public:

    typedef Value                                              mapped_type;
    typedef Key                                                key_type;
    typedef typename boost::add_reference< mapped_type >::type reference;
    typedef typename boost::add_const< mapped_type >::type     const_reference;

    concurrent_map( const Compare& c = Compare() )
        : BaseType( 1, c )
    {}

	//! This interface admits concurrent reads to find a default constructed value for a given key if there is a writer using this.
    reference operator [] ( const key_type& k )
    {
        iterator it = find( k );
        if( it == end() )
            it = insert( value_type( k, mapped_type() ) ).first;
        return (it->second);
    }
};

//! Declare a map type with a variable height parameter.
template <typename Key, typename Value, unsigned int MaxHeight, typename Compare = std::less< Key >, typename Alloc = std::allocator< Key > >
class skip_map : public lazy_concurrent_skip_list< detail::associative_map_traits< Key, Value, Compare, Alloc, MaxHeight, false > >
{
    typedef lazy_concurrent_skip_list< detail::associative_map_traits< Key, Value, Compare, Alloc, MaxHeight, false > > BaseType;

public:

    typedef Value                                              mapped_type;
    typedef Key                                                key_type;
    typedef typename boost::add_reference< mapped_type >::type reference;
    typedef typename boost::add_const< mapped_type >::type     const_reference;

    skip_map( const Compare& c = Compare() )
        : BaseType( 1, c )
    {}

	//! This interface admits concurrent reads to find a default constructed value for a given key if there is a writer using this.
    reference operator [] ( const key_type& k )
    {
        iterator it = find( k );
        if( it == end() )
            it = insert( value_type( k, mapped_type() ) ).first;
        return (it->second);
    }
};

}//! namespace stk;

#endif//! STK_CONTAINER_CONCURRENT_SKIP_LIST_HPP

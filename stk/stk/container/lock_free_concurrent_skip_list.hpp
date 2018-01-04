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

#ifndef STK_CONTAINER_LOCK_FREE_CONCURRENT_SKIP_LIST_HPP
#define STK_CONTAINER_LOCK_FREE_CONCURRENT_SKIP_LIST_HPP
#pragma once

#include <stk/container/concurrent_skip_list.hpp>
#include <stk/container/atomic_markable_ptr.hpp>

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

			using node_levels = atomic_markable_ptr<node>*;

			std::uint8_t     get_flags() const { return flags.load(/*std::memory_order_consume*/std::memory_order_acquire); }
			void			 set_flags(std::uint8_t f) { flags.store(f, std::memory_order_release); }
			bool			 is_head() const { return get_flags() & flag::Head; }
			bool			 is_marked_for_removal() const { return get_flags() & flag::MarkedForRemoval; }
			void			 set_is_head() { set_flags(std::uint8_t(get_flags() | flag::Head)); }
			void			 set_marked_for_removal() { set_flags(std::uint8_t(get_flags() | flag::MarkedForRemoval)); }
			const key_type&  key() const { return traits::resolve_key(value_); }
			value_type&      item() { return value_; }
            template <typename Value>
            void             set_value(Value&& v){ value_ = std::forward<Value>(v); }
			node_levels&     next() { return nexts; }
			atomic_ptr&      next(std::uint8_t i) { GEOMETRIX_ASSERT(i <= topLevel); return nexts[i]; }
			void             set_next(std::uint8_t i, node_ptr pNode, mark_type mark) { GEOMETRIX_ASSERT(i <= topLevel); return nexts[i].store(pNode, mark, std::memory_order_release); }
			std::uint8_t	 get_top_level() const { return topLevel; }

		private:

			friend class lf_skip_list_node_manager<traits>;

			template <typename Value>
			node(std::uint8_t topLevel, Value&& data, bool isHead)
				: value_(std::forward<Value>(data))
				, flags(!isHead ? 0 : 0 | flag::Head)
				, topLevel(topLevel)
			{
				GEOMETRIX_ASSERT(isHead == is_head());
				GEOMETRIX_ASSERT(!is_marked_for_removal());
				for (std::uint8_t i = 0; i <= topLevel; ++i)
					new(&nexts[i]) atomic_markable_ptr<node>(nullptr, 0);
			}

			value_type			       value_;
			std::atomic<std::uint8_t>  flags;
			std::uint8_t               topLevel;
			atomic_markable_ptr<node> nexts[0];//! C trick for tight dynamic arrays.
		};

		lf_skip_list_node_manager(key_compare p, allocator_type al)
			: AssociativeTraits(p, al)
			, m_scopeManager(std::make_shared<node_scope_manager>(al))
		{}

		template <typename Data>
		node_ptr create_node(Data&& v, std::uint8_t topLevel, bool isHead = false)
		{
			return m_scopeManager->create_node(v, topLevel, isHead);
		}

		void clone_head_node(node_ptr pHead, node_ptr pNew)
		{
			GEOMETRIX_ASSERT(pHead);
			pNew->set_flags(pHead->get_flags());
			for (std::size_t i = 0; i <= pHead->get_top_level(); ++i)
				pNew->next(i).store_raw(pHead->next(i).load_raw());
		}

		void destroy_node(node_ptr pNode)
		{
			m_scopeManager->destroy_node(pNode);
		}

		void really_destroy_node(node_ptr pNode)
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
				std::size_t aSize = sizeof(node) + (topLevel + 1) * sizeof(atomic_markable_ptr<node>);
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
				std::size_t aSize = sizeof(node) + (pNode->get_top_level() + 1) * sizeof(atomic_markable_ptr<node>);
				pNode->~node();
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
}//! namespace detail;

template <typename AssociativeTraits, typename LevelSelectionPolicy = skip_list_level_selector<AssociativeTraits::max_level::value+1>>
class lock_free_concurrent_skip_list : public detail::lf_skip_list_node_manager<AssociativeTraits>
{
	static_assert(AssociativeTraits::max_height::value > 1 && AssociativeTraits::max_height::value <= 64, "MaxHeight should be in the range [2, 64]");
public:
	using traits_type = AssociativeTraits;
	using size_type = typename traits_type::size_type;
    using key_type = typename traits_type::key_type;
    using value_type = typename traits_type::value_type;
    using key_compare = typename traits_type::key_compare;
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

        friend class lock_free_concurrent_skip_list< traits_type, level_selector >;

        node_iterator()
            : m_pNode()
        {}

		template <typename U>
		node_iterator(const node_iterator<U>& other)
			: m_pNode(other.m_pNode)
			, m_pNodeManager(other.m_pNodeManager)
		{
			if (m_pNode)
				acquire();
		}

		node_iterator& operator = (const node_iterator& rhs)
		{
			if (m_pNode && !rhs.m_pNode)
				release();
			m_pNodeManager = rhs.m_pNodeManager;
			if (!m_pNode && rhs.m_pNode)
				acquire();
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
		
		void acquire()
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

		iterator(const iterator&) = default;
		iterator& operator=(const iterator&) = default;
		iterator(iterator&&) = default;
		iterator& operator=(iterator&&) = default;

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

		const_iterator(const const_iterator&) = default;
		const_iterator& operator=(const const_iterator&) = default;
		const_iterator(const_iterator&&) = default;
		const_iterator& operator=(const_iterator&&) = default;

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

        auto found = find( x, preds, succs );
        if( found )
        {
            node_ptr pFound = succs[0];
            GEOMETRIX_ASSERT( pFound );
            if( pFound && !pFound->is_marked_for_removal() )
                return iterator(get_scope_manager(), pFound);
        }

        return end();
    }

    const_iterator find(const key_type& x) const
    {
        std::array<node_ptr, max_height::value> preds;
        std::array<node_ptr, max_height::value> succs;

        auto found = find( x, preds, succs );
        if( found )
        {
            auto pFound = succs[0];
            GEOMETRIX_ASSERT( pFound );
            if( pFound && !pFound->is_marked_for_removal() )
                return const_iterator(get_scope_manager(),pFound);
        }

        return end();
    }

    std::pair<iterator, bool> insert(const value_type& item)
    {
        return add( item );
    }

    iterator insert(const iterator&, const value_type& item)
    {
        return add( item ).first;
    }
    
    bool contains( const key_type& x ) const
    {
		int bottomLevel = 0;
		mark_type mark = 0;
		node_ptr pred = m_pHead.load(std::memory_order_acquire);
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
					curr = pred->next(level).get_ptr();
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
                    {
                        nodeToRemove->next(level).compare_exchange_weak(succ, mark, succ, 1);
                    }
                }
                succ = nodeToRemove->next(bottomLevel).get_ptr();
                while(true)
                {
					auto mark = mark_type{0};
                    bool iMarkedIt = nodeToRemove->next(bottomLevel).compare_exchange_strong(succ, mark, succ, 1);
					GEOMETRIX_ASSERT(nodeToRemove == succs[bottomLevel]);
                    //std::tie(succ, mark) = succs[bottomLevel]->next(bottomLevel).get();
                    if(iMarkedIt)
                    {
						nodeToRemove->set_marked_for_removal();
                        find(x, preds, succs);
                        destroy_node(nodeToRemove);
       	                decrement_size();
                        return iterator(get_scope_manager(), succs[bottomLevel]);
                    }
                    else if(mark)
                        return end();
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

    node_ptr left_most() const
    {
        return m_pHead.load(/*std::memory_order_consume*/std::memory_order_acquire)->next(0).get_ptr();
    }

    void release()
    {
        GEOMETRIX_ASSERT( m_pHead );

		auto pCurr = m_pHead.load(std::memory_order_relaxed);
		while (pCurr)
		{
			auto pNext = pCurr->next(0).get_ptr();
			really_destroy_node(pCurr);
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
        while(true)
        {
        retry:
            pred = m_pHead.load(std::memory_order_acquire);
            for(int level = max_level::value; level >= bottomLevel; --level)
            {
                curr = pred->next(level).get_ptr();
                while(curr)
                {
                    std::tie(succ, mark) = curr->next(level).get();
                    while(mark)
                    {
						auto expectedPtr = curr;
						auto expectedStamp = mark_type{ 0 };
                        snip = pred->next(level).compare_exchange_strong(expectedPtr, expectedStamp, succ, 0);
                        if(!snip) 
                            goto retry;
                        curr = pred->next(level).get_ptr();
						if (!curr)
							break;
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

	template <typename Value>
    std::pair<iterator, bool> add( Value&& x )
    {
		auto pHead = m_pHead.load(/*std::memory_order_consume*/std::memory_order_acquire);
		int topLevel = m_selector(pHead->get_top_level());
        int bottomLevel = 0;
        std::array<node_ptr, max_height::value> preds;
        std::array<node_ptr, max_height::value> succs;
		std::size_t newSize;
		node_ptr pNewNode;
		auto key = resolve_key(x);
        while( true )
        {
            bool found = find(key, preds, succs);
            if( found )
                return std::make_pair(iterator(get_scope_manager(), succs[bottomLevel]), false);
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
				auto expectedPtr = succ;
				auto expectedStamp = mark_type{ 0 };
                if( !pred->next(bottomLevel).compare_exchange_strong(expectedPtr, expectedStamp, pNewNode, 0) )
                {
                    really_destroy_node(pNewNode);
                    continue;
                }

                for(int level = bottomLevel+1; level <= topLevel; ++level)
                {
                    while(true)
                    {
                        pred = preds[level];
                        succ = succs[level];
						auto pExpected = succ;
						auto sExpected = mark_type{0};
                        if(pred->next(level).compare_exchange_strong(pExpected, sExpected, pNewNode, 0))
                            break;
                        find(key, preds, succs);
                    }
                }

			    newSize = increment_size();
                //auto tLvl = m_pHead.load(std::memory_order_relaxed)->get_top_level();
		        //if ((tLvl + 1) < max_height::value && newSize > detail::get_size_table()[tLvl])
			    //    increment_height(tLvl + 1);

                return std::make_pair(iterator(get_scope_manager(), pNewNode), true);
            }
        }
    }

    template <typename Value, typename Fn>
    std::pair<iterator, bool> add_or_update(Value&& x, Fn&& updateFn)
    {
		auto pHead = m_pHead.load(/*std::memory_order_consume*/std::memory_order_acquire);
		int topLevel = m_selector(pHead->get_top_level());
        int bottomLevel = 0;
        std::array<node_ptr, max_height::value> preds;
        std::array<node_ptr, max_height::value> succs;
		std::size_t newSize;
		node_ptr pNewNode;
		auto key = resolve_key(x);
        while( true )
        {
            bool found = find(key, preds, succs);
            if( found )
            {
                iterator it(get_scope_manager(), succs[bottomLevel]);
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
				auto expectedPtr = succ;
				auto expectedStamp = mark_type{ 0 };
                if( !pred->next(bottomLevel).compare_exchange_strong(expectedPtr, expectedStamp, pNewNode, 0) )
                {
                    really_destroy_node(pNewNode);
                    continue;
                }
				
				updateFn(true, pNewNode->item());

                for(int level = bottomLevel+1; level <= topLevel; ++level)
                {
                    while(true)
                    {
                        pred = preds[level];
                        succ = succs[level];
						auto pExpected = succ;
						auto sExpected = mark_type{0};
                        if(pred->next(level).compare_exchange_strong(pExpected, sExpected, pNewNode, 0))
                            break;
                        find(key, preds, succs);
                    }
                }

			    newSize = increment_size();
                //auto tLvl = m_pHead.load(std::memory_order_relaxed)->get_top_level();
		        //if ((tLvl + 1) < max_height::value && newSize > detail::get_size_table()[tLvl])
			    //    increment_height(tLvl + 1);
                return std::make_pair(iterator(get_scope_manager(), pNewNode), true);
            }
        }
    }

	size_type increment_size() { return m_size.fetch_add(1, std::memory_order_relaxed) + 1; }
	size_type decrement_size() { return m_size.fetch_sub(1, std::memory_order_relaxed) - 1; }

	//! Not lock free or even 'working'.
	void increment_height(std::uint8_t tLvl)
	{
		auto pHead = m_pHead.load(/*std::memory_order_consume*/std::memory_order_acquire);
		if (pHead->get_top_level() >= tLvl)
			return;
		GEOMETRIX_ASSERT(tLvl > pHead->get_top_level());
		auto pNew = create_node(value_type(), tLvl, true);
		{
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
class lock_free_concurrent_set : public lock_free_concurrent_skip_list< detail::associative_set_traits< Key, Compare, Alloc, 32, false > >
{
    typedef lock_free_concurrent_skip_list< detail::associative_set_traits< Key, Compare, Alloc, 32, false > > BaseType;

public:

    lock_free_concurrent_set( const Compare& c = Compare() )
        : BaseType( max_level::value, c )
    {}
};

//! Declare a map type.
template <typename Key, typename Value, typename Compare = std::less< Key >, typename Alloc = std::allocator< Key > >
class lock_free_concurrent_map : public lock_free_concurrent_skip_list< detail::associative_map_traits< Key, Value, Compare, Alloc, 32, false > >
{
    typedef lock_free_concurrent_skip_list< detail::associative_map_traits< Key, Value, Compare, Alloc, 32, false > > BaseType;

public:

    typedef Value                                              mapped_type;
    typedef Key                                                key_type;
    typedef typename boost::add_reference< mapped_type >::type reference;
    typedef typename boost::add_const< mapped_type >::type     const_reference;

    lock_free_concurrent_map( const Compare& c = Compare() )
        : BaseType( max_level::value, c )
    {}

	//! This interface admits concurrent reads to find a default constructed value for a given key if there is a writer using this.
    reference operator [] ( const key_type& k )
    {
        iterator it = find( k );
        if( it == end() )
            it = insert( value_type( k, mapped_type() ) ).first;
        return (it->second);
    }
    
    template <typename UpdateFn>
    std::pair<iterator, bool> insert_or_update(const key_type& key, UpdateFn&& fn)
    {
        return add_or_update(value_type(key, mapped_type()), std::forward<UpdateFn>(fn));
    }

};

//! Declare a map type with a variable height parameter.
template <typename Key, typename Value, unsigned int MaxHeight, typename Compare = std::less< Key >, typename Alloc = std::allocator< Key > >
class lock_free_skip_map : public lock_free_concurrent_skip_list< detail::associative_map_traits< Key, Value, Compare, Alloc, MaxHeight, false > >
{
    typedef lock_free_concurrent_skip_list< detail::associative_map_traits< Key, Value, Compare, Alloc, MaxHeight, false > > BaseType;

public:

    typedef Value                                              mapped_type;
    typedef Key                                                key_type;
    typedef typename boost::add_reference< mapped_type >::type reference;
    typedef typename boost::add_const< mapped_type >::type     const_reference;

    lock_free_skip_map( const Compare& c = Compare() )
        : BaseType( MaxHeight - 1, c )
    {}

	//! This interface admits concurrent reads to find a default constructed value for a given key if there is a writer using this.
    reference operator [] ( const key_type& k )
    {
        iterator it = find( k );
        if( it == end() )
            it = insert( value_type( k, mapped_type() ) ).first;
        return (it->second);
    }
    
    template <typename UpdateFn>
    std::pair<iterator, bool> insert_or_update(const key_type& key, UpdateFn&& fn)
    {
        return add_or_update(value_type(key, mapped_type()), std::forward<UpdateFn>(fn));
    }

};

}//! namespace stk;

#endif//! STK_CONTAINER_LOCK_FREE_CONCURRENT_SKIP_LIST_HPP


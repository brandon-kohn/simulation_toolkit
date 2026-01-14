//
//! Copyright © 2022

//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include <vector>
#include <atomic>
#include <memory>
#include <array>
#include <mutex>
#include <type_traits>
#include <condition_variable>
#include <stk/thread/concurrentqueue.h>
#include <stk/utility/make_vector.hpp>
#include <geometrix/utility/assert.hpp>
#include <boost/config.hpp>
#include <stk/thread/race_detector.hpp>
#include <thread>
#include <cstddef>
#include <chrono>

namespace stk {

	template <std::size_t Factor>
	struct constant_growth_policy
	{
		std::size_t initial_size() const { return Factor; }
		std::size_t growth_factor( std::size_t ) const { return Factor; }
	};
	template <std::size_t InitialFactor>
	struct geometric_growth_policy
	{
		std::size_t initial_size() const { return InitialFactor; }
		std::size_t growth_factor( std::size_t capacity ) const { return 2 * capacity; }
	};

	template <class T>
	struct pool_access
	{
		template <class... Args>
		static T* construct( void* mem, Args&&... args )
		{
			return ::new( mem ) T( std::forward<Args>( args )... );
		}

		static void destroy( T* p ) noexcept
		{
			if constexpr( !std::is_trivially_destructible_v<T> )
				p->~T();
		}
	};

	template <typename T>
	class memory_pool_base
	{
	protected:
		using value_type = T;
		struct node
		{
			node()
				: storage{}
				, pool{}
			{}

			std::aligned_storage_t<sizeof( value_type ), alignof( value_type )> storage;
			memory_pool_base<T>*                                                pool;
		};
		using block = std::vector<node>;
		static_assert( std::is_standard_layout_v<node> );
		static_assert( offsetof( node, storage ) == 0 );

	public:
		
		static memory_pool_base* get_pool( const value_type* v )
		{
			auto* n = reinterpret_cast<const node*>( v );
			return n->pool;
		}

		memory_pool_base( std::size_t initialSize = 8 )
			: m_blocks{ stk::make_vector<block>( block( initialSize ) ) }
			, m_totalElements{ initialSize }
		{
			auto& blk = m_blocks.front();
			auto  it = blk.begin();
			auto  gen = [&]()
			{ return &*it++; };
			m_q.generate_bulk( gen, blk.size() );
		}

		template <typename... Args>
		static value_type* construct(void* mem, Args&&... a)
		{
			GEOMETRIX_ASSERT(mem != nullptr);
			return stk::pool_access<value_type>::construct(mem, std::forward<Args>(a)...);
		}

		static void destroy(value_type* v)
		{
			stk::pool_access<value_type>::destroy(v);
		}

		static void destroy( const value_type* v )
		{
			destroy( const_cast<value_type*>( v ) );
		}

		void deallocate( value_type* v )
		{
			enqueue( reinterpret_cast<node*>( v ) );
		}

		void deallocate( const value_type* v )
		{
			enqueue( reinterpret_cast<node*>( const_cast<value_type*>(v) ) );
		}

		std::size_t size_free() const { return m_q.size_approx(); }
		std::size_t size_elements() const
		{
			return m_totalElements.load( std::memory_order_acquire );
		}

	protected:
		void enqueue( node* n )
		{
			n->pool = nullptr;
			m_q.enqueue( n );
			m_itemsLoaded.notify_one();
		}

		std::vector<block>                 m_blocks;
		moodycamel::ConcurrentQueue<node*> m_q;
		std::mutex                         m_loadMutex;
		std::condition_variable            m_itemsLoaded;
		std::atomic<std::size_t>           m_totalElements;
	};

	template <typename T, typename GrowthPolicy = geometric_growth_policy<100>>
	class memory_pool : public memory_pool_base<T>
	{
		using base_t = memory_pool_base<T>;
		using node = typename base_t::node;
		using value_type = T;
		using growth_policy = GrowthPolicy;

	public:
		memory_pool()
			: base_t{ growth_policy().initial_size() }
			, m_isExpanding{false}
		{
		}

		value_type* allocate()
		{
			auto* mem = dequeue();
			return mem;
		}

	private:
		value_type* dequeue()
		{
			node* n = nullptr;
			while( !base_t::m_q.try_dequeue( n ) )
				expand();
			if( n )
			{
				n->pool = this;
				return std::launder( reinterpret_cast<value_type*>( &n->storage ) );
			}
			return nullptr;
		}

		void expand()
		{
			bool isExpanding = false;
			if( m_isExpanding.compare_exchange_strong( isExpanding, true, std::memory_order_acq_rel ) )
			{
				{
					STK_DETECT_RACE( racer );
					if( base_t::m_q.size_approx() == 0 )
					{
						auto growSize = growth_policy().growth_factor( base_t::m_blocks.back().size() );
						base_t::m_blocks.emplace_back( growSize );
						auto it = base_t::m_blocks.back().begin();
						auto gen = [&]()
						{ return &*it++; };
						base_t::m_q.generate_bulk( gen, growSize );
						m_totalElements.fetch_add( growSize, std::memory_order_release );
					}
				}
				m_isExpanding.store( false, std::memory_order_release );
				base_t::m_itemsLoaded.notify_all();
			}
			else
			{
				auto lk = std::unique_lock<std::mutex>{ base_t::m_loadMutex };
				auto doneExpanding = [this]() { return !m_isExpanding.load( std::memory_order_acquire ); };
				constexpr int nBackoffs = 1000;
				for( int i = 0; i < nBackoffs && !doneExpanding(); ++i )
					std::this_thread::yield();

				if( !doneExpanding() )
					base_t::m_itemsLoaded.wait( lk, doneExpanding );
			}	
		}

		std::atomic<bool> m_isExpanding;
        STK_RACE_DETECTOR(racer);
	};

	template <typename U>
	inline void deallocate_to_pool( U* v )
	{
		using value_type = typename std::decay<U>::type;
		auto* pool = memory_pool_base<value_type>::get_pool( v );
		GEOMETRIX_ASSERT( pool != nullptr );
		pool->deallocate( v );
	}

	template <typename U>
	inline void destroy_and_deallocate_to_pool( U* v )
	{
		using value_type = std::decay_t<U>;
		auto* pool = memory_pool_base<value_type>::get_pool( v );
		GEOMETRIX_ASSERT( pool != nullptr );
		memory_pool_base<value_type>::destroy( v );
		pool->deallocate( v );
	}

	template <typename U, typename... Args>
	inline U* make_from_pool( memory_pool<U>& pool, Args&&... args )
	{
		auto* mem = pool.allocate();
		return memory_pool_base<U>::construct( mem, std::forward<Args>( args )... );
	}

	//! Deleter for use with std::unique_ptr and std::shared_ptr to automatically manage memory from a memory pool.
	template <typename T>
	struct pool_deleter
	{
		void operator()( T* p ) const noexcept
		{
			if( !p )
				return;
			auto* pool = memory_pool_base<T>::get_pool( p );
			GEOMETRIX_ASSERT( pool != nullptr );
			memory_pool_base<T>::destroy( p );
			pool->deallocate( p );
		}
	};

	template <typename T>
	using pooled_ptr = std::unique_ptr<T, pool_deleter<T>>;

	template <typename U, typename... Args>
	inline pooled_ptr<U> make_unique_from_pool( memory_pool<U>& pool, Args&&... args )
	{
		auto* mem = pool.allocate();
		auto* obj = memory_pool_base<U>::construct( mem, std::forward<Args>( args )... );
		return pooled_ptr<U>{ obj };
	}

	//! Object pool used when destructor management is not required.
	//! Useful for objects that are default constructed once and then reused many times without needing to destroy/construct each cycle.
	template <class T, class GrowthPolicy = geometric_growth_policy<100>>
	class object_pool : private memory_pool_base<T>
	{
		using base_t = memory_pool_base<T>;
		using node = typename base_t::node;
		using growth_policy = GrowthPolicy;

	public:
		object_pool( std::size_t initialSize = growth_policy().initial_size() )
			: base_t{ initialSize }
		{
			//! IMPORTANT: base_t enqueued nodes already; so construct immediately
			//! before the pool can be used by any other thread.
			construct_block( base_t::m_blocks.front() );
		}

		T* allocate()
		{
			node* n = nullptr;
			while( !base_t::m_q.try_dequeue( n ) )
				expand();

			n->pool = this;
			return std::launder( reinterpret_cast<T*>( &n->storage ) );
		}

		void deallocate( const T* p ) { base_t::deallocate( p ); }

	private:
		static void construct_block( typename base_t::block& blk )
		{
			for( auto& n : blk )
				base_t::construct( &n.storage );
		}

		void expand()
		{
			bool expected = false;
			if( m_isExpanding.compare_exchange_strong( expected, true, std::memory_order_acq_rel ) )
			{
				//! Expand only if still empty (heuristic; ok)
				if( base_t::m_q.size_approx() == 0 )
				{
					auto growSize = growth_policy().growth_factor( base_t::m_blocks.back().size() );
					base_t::m_blocks.emplace_back( growSize );

					auto& blk = base_t::m_blocks.back();
					construct_block( blk );

					//! Queue after construct!!
					auto it = blk.begin();
					auto gen = [&]() { return &*it++; };
					base_t::m_q.generate_bulk( gen, blk.size() );
				}

				m_isExpanding.store( false, std::memory_order_release );
				base_t::m_itemsLoaded.notify_all();
			}
			else
			{
				auto lk = std::unique_lock<std::mutex>{ base_t::m_loadMutex };
				auto done = [this]
				{ return !m_isExpanding.load( std::memory_order_acquire ); };

				//! small backoff then block
				constexpr int nBackoffs = 1000;
				for( int i = 0; i < nBackoffs && !done(); ++i )
					std::this_thread::yield();

				if( !done() )
					base_t::m_itemsLoaded.wait( lk, done );
			}
		}

		std::atomic<bool> m_isExpanding{ false };
	};

}//! namespace stk;

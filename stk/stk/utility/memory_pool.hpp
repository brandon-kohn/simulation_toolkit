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

	public:
		static memory_pool_base* get_pool( value_type* v )
		{
			auto* n = reinterpret_cast<node*>( v );
			return n->pool;
		}

		memory_pool_base( std::size_t initialSize = 8 )
			: m_blocks{ stk::make_vector<block>( block( initialSize ) ) }
		{
			auto& blk = m_blocks.front();
			auto  it = blk.begin();
			auto  gen = [&]()
			{ return &*it++; };
			m_q.generate_bulk( gen, blk.size() );
		}

		template <typename... Args>
		static value_type* construct( void* mem, Args&&... a )
		{
			GEOMETRIX_ASSERT( mem != nullptr );
			::new( mem ) value_type( std::forward<Args>( a )... );
			return reinterpret_cast<value_type*>( mem );
		}

		static void destroy( value_type* v )
		{
			if constexpr( std::is_destructible<value_type>::value )
			{
				v->~value_type();
			}
		}

		void deallocate( value_type* v )
		{
			enqueue( reinterpret_cast<node*>( v ) );
		}

		std::size_t size_free() const { return m_q.size_approx(); }
		std::size_t size_elements() const
		{
			auto size = std::size_t{};
			for( const auto& blk : m_blocks )
			{
				size += blk.size();
			}
			return size;
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
			if( m_isExpanding.compare_exchange_strong( isExpanding, true ) )
			{
				if( base_t::m_q.size_approx() == 0 )
				{
					auto growSize = growth_policy().growth_factor( base_t::m_blocks.back().size() );
					base_t::m_blocks.emplace_back( growSize );
					auto it = base_t::m_blocks.back().begin();
					auto gen = [&]()
					{ return &*it++; };
					base_t::m_q.generate_bulk( gen, growSize );
				}
				m_isExpanding.store( false );
				base_t::m_itemsLoaded.notify_all();
			}
			else if( base_t::m_q.size_approx() == 0 )
			{
				auto lk = std::unique_lock<std::mutex>{ base_t::m_loadMutex };
				base_t::m_itemsLoaded.wait( lk, [this]()
					{ return !m_isExpanding.load( std::memory_order_relaxed ) || base_t::m_q.size_approx() > 0; } );
			}
		}

		std::atomic<bool> m_isExpanding;
	};

	template <typename U>
	inline void deallocate_to_pool( U* v )
	{
		auto* pool = memory_pool_base<typename std::decay<U>::type>::get_pool( v );
		GEOMETRIX_ASSERT( pool != nullptr );
		pool->deallocate( v );
	}
}//! namespace stk;

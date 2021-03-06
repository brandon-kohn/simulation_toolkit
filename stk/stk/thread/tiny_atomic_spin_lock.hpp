//
//! Copyright � 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/thread/null_wait_strategy.hpp>
#include <geometrix/utility/assert.hpp>
#include <atomic>
#include <cstdint>

namespace stk { namespace thread {
	
	//! Conforms to the Lockable concept and can be used as a mutex.
	template <typename WaitStrategy = null_wait_strategy>
	class tiny_atomic_spin_lock
	{
		enum lock_state : std::uint8_t
		{
			Free = 0
		,   Locked = 1
		};

	public:
		
		tiny_atomic_spin_lock() = default;
		
		void lock()
		{
			WaitStrategy wait;
			do 
			{
				while (m_state.load() != Free)
					wait();
			} 
			while (!try_lock());
			GEOMETRIX_ASSERT(m_state.load() == Locked);
		}
		
		bool try_lock()
		{
			std::uint8_t expected = Free;
			return m_state.compare_exchange_strong(expected, Locked, std::memory_order_acquire, std::memory_order_relaxed);
		}
		
		void unlock()
		{
			m_state.store(Free, std::memory_order_release);
		}

	private:

		std::atomic<std::uint8_t> m_state{ Free };

	};

}}//! namespace stk::thread;


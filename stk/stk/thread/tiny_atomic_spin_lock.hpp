//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_THREAD_TINY_ATOMIC_SPIN_LOCK_HPP
#define STK_THREAD_TINY_ATOMIC_SPIN_LOCK_HPP

#include <stk/thread/spin_lock_wait_strategies.hpp>
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
			std::uint8_t desired = Locked;
			return std::atomic_compare_exchange_strong_explicit(&m_state, &expected, desired, std::memory_order_acquire, std::memory_order_relaxed);
		}
		
		void unlock()
		{
			m_state.store(Free, std::memory_order_release);
		}

	private:

		std::atomic<std::uint8_t> m_state{ Free };

	};

}}//! namespace stk::thread;

#endif//! STK_THREAD_TINY_ATOMIC_SPIN_LOCK_HPP 

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

namespace stk { namespace thread {
	
	//! Conforms to the Lockable concept and can be used as a mutex.
	template <typename WaitStrategy = null_wait_strategy>
	class tiny_atomic_spin_lock
	{
		enum lock_state
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
	  }
  
	  bool try_lock()
	  {
		  return compare_and_set(Free, Locked);
	  }
  
	  void unlock()
	  {
		  m_state.store(Free, std::memory_order_release);
	  }
  
	private:

		bool compare_and_set(std::uint8_t expected, std::uint8_t result)
		{
			return std::atomic_compare_exchange_strong_explicit(&m_state, &expected, result, std::memory_order_acquire, std::memory_order_relaxed);
		}

		std::atomic<std::uint8_t> m_state{ Free };

	};

}}//! namespace stk::thread;

#endif//! STK_THREAD_TINY_ATOMIC_SPIN_LOCK_HPP 

//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_THREAD_ATOMIC_SPIN_LOCK_HPP
#define STK_THREAD_ATOMIC_SPIN_LOCK_HPP

#include <stk/thread/spin_lock_wait_strategies.hpp>
#include <atomic>

#define STK_TRACK_LOCK_OWNER 1
#if STK_TRACK_LOCK_OWNER
#include <boost/optional.hpp>
#include <thread>
#endif

namespace stk { namespace thread {

//! Also conforms to the Lockable concept and can be used as a mutex.
template <typename WaitStrategy = null_wait_strategy>
class atomic_spin_lock
{
public:

  atomic_spin_lock() = default;

  void lock()
  {
	  WaitStrategy wait;
      while(m_state.test_and_set(std::memory_order_acquire)) wait();

#if STK_TRACK_LOCK_OWNER
	  m_id = std::this_thread::get_id();
#endif
  }
  
  bool try_lock()
  {
      auto r = !m_state.test_and_set(std::memory_order_acquire);
#if STK_TRACK_LOCK_OWNER
	  if( r )
		  m_id = std::this_thread::get_id();
#endif
	  return r;
  }
  
  void unlock()
  {
      m_state.clear(std::memory_order_release);
#if STK_TRACK_LOCK_OWNER
	  m_id.reset();
#endif
  }
  
private:

    std::atomic_flag m_state = ATOMIC_FLAG_INIT;
#if STK_TRACK_LOCK_OWNER
	boost::optional<std::thread::id> m_id;
#endif

};

}}//! namespace stk::thread;

#endif//! STK_THREAD_ATOMIC_SPIN_LOCK_HPP

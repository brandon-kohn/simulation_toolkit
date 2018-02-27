//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_THREAD_SPIN_LOCK_WAIT_STRATEGIES_HPP
#define STK_THREAD_SPIN_LOCK_WAIT_STRATEGIES_HPP
#pragma once

#include <boost/fiber/operations.hpp>
#include <stk/thread/boost_thread_kernel.hpp>
#include <stk/thread/std_thread_kernel.hpp>

namespace stk {	namespace thread {

	struct null_wait_strategy 
	{
		void operator()()
		{
		
		}
	};

	template <unsigned int N>
	struct eager_fiber_yield_wait
	{
		void operator()()
		{
			if (++count > N)
				boost::this_fiber::yield();
		}

		unsigned int count{ 0 };
	};

	struct fiber_yield_wait
	{
		void operator()()
		{
			boost::this_fiber::yield();
		}
	};

	template <unsigned int N>
	struct eager_boost_thread_yield_wait
	{
		void operator()()
		{
			if (++count > N)
				boost::this_thread::yield();
		}

		unsigned int count{ 0 };
	};

	struct boost_thread_yield_wait
	{
		void operator()()
		{
			boost::this_thread::yield();
		}
	};

}}//! namespace stk::thread;

#endif//! STK_THREAD_SPIN_LOCK_WAIT_STRATEGIES_HPP

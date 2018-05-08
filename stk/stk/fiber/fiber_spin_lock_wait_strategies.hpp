//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/fiber/operations.hpp>

namespace stk {	namespace thread {

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

}}//! namespace stk::thread;


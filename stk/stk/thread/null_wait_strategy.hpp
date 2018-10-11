//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

namespace stk {	namespace thread {

	struct null_wait_strategy 
	{
		void operator()()
		{
		
		}
	};

}}//! namespace stk::thread;


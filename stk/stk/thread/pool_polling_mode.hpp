//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_THREAD_POOL_POLLING_MODE_HPP
#define STK_THREAD_POOL_POLLING_MODE_HPP
#pragma once

#include <cstdint>

namespace stk { namespace thread {

	enum class polling_mode : std::uint8_t
	{
		fast//! Use this mode when there are always tasks in the queue.
	  , wait//! Use this mode when there is significant time between tasks being in the queue.
	};

}}//! namespace stk::thread;

#endif//! STK_THREAD_POOL_POLLING_MODE_HPP

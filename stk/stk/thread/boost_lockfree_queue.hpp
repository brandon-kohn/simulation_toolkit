//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/lockfree/queue.hpp>

struct boost_lockfree_queue_traits
{
	template <typename T, typename Alloc = std::allocator<T>, typename Mutex = std::mutex>
	using queue_type = boost::lockfree::queue<T>;

	template <typename T, typename Value>
	static bool try_push(queue_type<T>& q, Value&& value)
	{
		return q.push(std::forward<Value>(value));
	}

	template <typename T>
	static bool try_pop(queue_type<T>& q, T& value)
	{
		return q.pop(value);
	}

	template <typename T>
	static bool try_steal(queue_type<T>& q, T& value)
	{
		return q.pop(value);
	}
};


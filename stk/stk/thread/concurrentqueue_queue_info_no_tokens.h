#pragma once

#include <stk/thread/concurrentqueue.h>
#include <stk/utility/none.hpp>

struct moodycamel_concurrent_queue_traits_no_tokens
{
	template <typename T, typename Alloc = std::allocator<T>, typename Mutex = std::mutex>
	using queue_type = moodycamel::ConcurrentQueue<T>;
	using queue_info = stk::none_type;

	template <typename T>
	static queue_info get_queue_info(queue_type<T>&)
	{
		return stk::none;
	}

	template <typename T, typename Value>
	static bool try_push(queue_type<T>& q, queue_info, Value&& value)
	{
		return q.enqueue(std::forward<Value>(value));
	}

	template <typename T>
	static bool try_pop(queue_type<T>& q, queue_info, T& value)
	{
		return q.try_dequeue(value);
	}

	template <typename T>
	static bool try_steal(queue_type<T>& q, queue_info, T& value)
	{
		return q.try_dequeue(value);
	}

	template <typename T, typename Value>
	static bool try_push(queue_type<T>& q, Value&& value)
	{
		return q.enqueue(std::forward<Value>(value));
	}

	template <typename T>
	static bool try_pop(queue_type<T>& q, T& value)
	{
		return q.try_dequeue(value);
	}

	template <typename T>
	static bool try_steal(queue_type<T>& q, T& value)
	{
		return q.try_dequeue(value);
	}
};


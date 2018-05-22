#pragma once

#include <stk/thread/concurrentqueue.h>
#include <stk/thread/thread_specific.hpp>
STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(moodycamel::queue_info);
template <typename T>
struct concurrent_queue_wrapper
{
    concurrent_queue_wrapper(std::size_t nSize = 6 * moodycamel::ConcurrentQueueDefaultTraits::BLOCK_SIZE)
        : m_q(nSize)
		, m_queue_info([this]() { return moodycamel::queue_info(m_q); })
    {}

	moodycamel::queue_info* get_queue_info()
	{
		return &*m_queue_info;
	}

	template <typename Value>
	bool enqueue(Value&& value)
	{
		moodycamel::ProducerToken& token = m_queue_info->ptoken;
		return m_q.enqueue(token, std::forward<Value>(value));
	}

	template <typename Value>
	bool enqueue(moodycamel::ProducerToken& ptoken, T&& value)
	{
		return m_q.enqueue(ptoken, std::forward<Value>(value));
	}

	bool try_dequeue(T& value)
	{
		moodycamel::ConsumerToken& token = m_queue_info->ctoken;
		return m_q.try_dequeue(token, value);
	}

	bool try_dequeue(moodycamel::ConsumerToken& ctoken, T& value)
	{
		return m_q.try_dequeue(ctoken, value);
	}

	moodycamel::ConcurrentQueue<T> m_q;
	stk::thread::thread_specific<moodycamel::queue_info> m_queue_info;
};

struct moodycamel_concurrent_queue_traits
{
	template <typename T, typename Alloc = std::allocator<T>, typename Mutex = std::mutex>
	using queue_type = concurrent_queue_wrapper<T>;
	using queue_info = moodycamel::queue_info*;

	template <typename T>
	static queue_info get_queue_info(queue_type<T>&q)
	{
		return q.get_queue_info();
	}

	template <typename T, typename Value>
	static bool try_push(queue_type<T>& q, queue_info queueInfo, Value&& value)
	{
		return q.enqueue(queueInfo->ptoken, std::forward<Value>(value));
	}

	template <typename T>
	static bool try_pop(queue_type<T>& q, queue_info queueInfo, T& value)
	{
		return q.try_dequeue(queueInfo->ctoken, value);
	}

	template <typename T>
	static bool try_steal(queue_type<T>& q, queue_info queueInfo, T& value)
	{
		return q.try_dequeue(queueInfo->ctoken, value);
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


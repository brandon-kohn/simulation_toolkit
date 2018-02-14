//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_LOCKED_QUEUE_HPP
#define STK_LOCKED_QUEUE_HPP
#pragma once

#include <condition_variable>
#include <mutex>
#include <limits>
#include <deque>

namespace stk {

	template <typename Item, typename Alloc = std::allocator<Item>, typename Mutex = std::mutex>
	class locked_queue
	{
		using mutex_type = Mutex;

	public:

		typedef std::deque<Item, Alloc> queue_type;
		typedef Item value_type;

		locked_queue(std::size_t maxSize = (std::numeric_limits<std::size_t>::max)())
			: m_maxSize(maxSize)
		{}

		locked_queue(std::size_t maxSize, const Alloc& alloc)
			: m_maxSize(maxSize)
			, m_queue(alloc)
		{}

		locked_queue(const locked_queue& rhs)
			: m_maxSize(rhs.m_maxSize)
			, m_queue(rhs.m_queue)
		{}

		//! A waiting push operation. If the Q is full will wait until it has room before inserting and then returning.
		void push_or_wait(const Item& x)
		{
			push_or_wait_impl(static_cast<const Item&>(x));
		}

		//! A waiting push operation. If the Q is full will wait until it has room before inserting and then returning.
		void push_or_wait(Item &&x)
		{
			push_or_wait_impl(std::forward<Item>(x));
		}

		//! A non waiting push operation. If the Q is full returns false.
		//! @return bool - whether the insertion was successful.
		bool try_push(const Item& x)
		{
			return try_push_impl(static_cast<const Item&>(x));
		}

		//! A non waiting push operation. If the Q is full returns false.
		//! @return bool - whether the insertion was successful.
		bool try_push(Item &&x)
		{
			return try_push_impl(std::forward<Item>(x));
		}

		//! A waiting pop operation which returns the front item immediately if the Q is not empty. If the Q is empty, it blocks until an element is available.
		//! @return value_type
		Item pop_or_wait()
		{
			std::unique_lock<mutex_type> lock(m_mutex);

			while (m_queue.empty())
				m_empty.wait(lock);

			Item item = std::move(m_queue.front());
			m_queue.pop_front();

			if (m_queue.size() < m_maxSize)
			{
				lock.unlock();
				m_full.notify_one();
			}

			return std::move(item);
		}

		//! A pop operation which gets the front item if the Q is not empty. If the Q is empty, returns false.
		//! @return - whether an item was popped.
		bool try_pop(Item& item)
		{
			std::unique_lock<mutex_type> lock(m_mutex);

			if (m_queue.empty())
				return false;

			item = std::move(m_queue.front());
			m_queue.pop_front();

			if (m_queue.size() < m_maxSize)
			{
				lock.unlock();
				m_full.notify_one();
			}

			return true;
		}

		//! A steal operation which gets the back item if the Q is not empty. If the Q is empty, returns false.
		//! @return - whether an item was stolen.
		bool try_steal(Item& item)
		{
			std::unique_lock<mutex_type> lock(m_mutex);

			if (m_queue.empty())
				return false;

			item = std::move(m_queue.back());
			m_queue.pop_back();

			if (m_queue.size() < m_maxSize)
			{
				lock.unlock();
				m_full.notify_one();
			}

			return true;
		}

		std::size_t size() const
		{
			std::unique_lock<mutex_type> lock(m_mutex);
			return m_queue.size();
		}

		bool empty() const
		{
			std::unique_lock<mutex_type> lock(m_mutex);
			return m_queue.empty();
		}

	private:

		//! A waiting push operation. If the Q is full will wait until it has room before inserting and then returning.
		template <typename U>
		void push_or_wait_impl(U&& item)
		{
			std::unique_lock<mutex_type> lock(m_mutex);

			while (m_queue.size() == m_maxSize)
				m_full.wait(lock);
			GEOMETRIX_ASSERT(lock.owns_lock());

			m_queue.emplace_back(std::forward<U>(item));

			if (m_queue.size() == 1)
			{
				lock.unlock();
				m_empty.notify_one();
			}
		}

		//! A non waiting push operation. If the Q is full returns false.
		//! @return bool - whether the insertion was successful.
		template <typename U>
		bool try_push_impl(U&& item)
		{
			std::unique_lock<mutex_type> lock(m_mutex);

			if (m_queue.size() == m_maxSize)
				return false;

			m_queue.emplace_back(std::forward<U>(item));

			if (m_queue.size() == 1)
			{
				lock.unlock();
				m_empty.notify_one();
			}

			return true;
		}

		std::condition_variable_any m_full;
		std::condition_variable_any m_empty;
		mutable mutex_type			m_mutex;
		std::size_t					m_maxSize;
		queue_type					m_queue;

	};

	struct locked_queue_traits
	{
		template <typename T, typename Alloc = std::allocator<T>, typename Mutex = std::mutex>
		using queue_type = locked_queue<T, Alloc, Mutex>;

		template <typename T, typename Alloc, typename Mutex, typename Value>
		static bool try_push(locked_queue<T, Alloc, Mutex>& q, Value&& value)
		{
			q.push_or_wait(std::forward<Value>(value));
			return true;
		}

		template <typename T, typename Alloc, typename Mutex>
		static bool try_pop(locked_queue<T, Alloc, Mutex>& q, T& value)
		{
			return q.try_pop(value);
		}

		template <typename T, typename Alloc, typename Mutex>
		static bool try_steal(locked_queue<T, Alloc, Mutex>& q, T& value)
		{
			return q.try_steal(value);
		}
	};


}//! namespace stk;

#endif //! STK_LOCKED_QUEUE_HPP

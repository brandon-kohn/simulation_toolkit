//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_THREAD_WORK_STEALING_THREAD_POOL_HPP
#define STK_THREAD_WORK_STEALING_THREAD_POOL_HPP
#pragma once

#include <stk/thread/function_wrapper.hpp>
#include <stk/thread/barrier.hpp>
#include <stk/thread/thread_specific.hpp>
#include <stk/container/locked_queue.hpp>
#include <stk/thread/boost_thread_kernel.hpp>
#include <stk/utility/scope_exit.hpp>
#include <stk/thread/pool_polling_mode.hpp>

#include <boost/noncopyable.hpp>
#include <boost/range/algorithm/for_each.hpp>

#include <memory>

namespace stk { namespace thread {

	template <typename QTraits = locked_queue_traits, typename Traits = boost_thread_traits>
	class work_stealing_thread_pool : boost::noncopyable
	{
		using thread_traits = Traits;
		using queue_traits = QTraits;

		using thread_type = typename thread_traits::thread_type;

		template <typename T>
		using packaged_task = typename Traits::template packaged_task_type<T>;

		using mutex_type = typename thread_traits::mutex_type;
		using queue_type = typename queue_traits::template queue_type<function_wrapper, std::allocator<function_wrapper>, mutex_type>;
		using queue_ptr = std::unique_ptr<queue_type>;

		void worker_thread(unsigned int tIndex)
		{
			if (m_onThreadStart)
				m_onThreadStart();

			m_nThreads.fetch_add(1, std::memory_order_relaxed);
			m_active.fetch_add(1, std::memory_order_relaxed);
			STK_SCOPE_EXIT(
				m_active.fetch_sub(1, std::memory_order_relaxed);
				m_nThreads.fetch_sub(1, std::memory_order_relaxed);
				if (m_onThreadStop)
					m_onThreadStop();
			);

			m_workQIndex = tIndex;
			function_wrapper task;

		polling_mode_fast:
			{
				while (!m_done.load(std::memory_order_relaxed))
				{
					if (poll(tIndex, task))
						task();
					else if (check_suspend_polling(tIndex, task));
					else if (m_pollingMode.load(std::memory_order_relaxed) == polling_mode::wait)
						goto polling_mode_wait;
					else
						thread_traits::yield();
				}
				return;
			}
		polling_mode_wait:
			{
				while (!m_done.load(std::memory_order_relaxed))
				{
					if (poll(tIndex, task))
						task();
					else if (check_suspend_polling(tIndex, task));
					else if (m_pollingMode.load(std::memory_order_relaxed) == polling_mode::fast)
							goto polling_mode_fast;
					else
					{
						m_active.fetch_sub(1, std::memory_order_relaxed);
						auto lk = std::unique_lock<mutex_type>{ m_pollingMtx };
						m_pollingCnd.wait_for(lk, std::chrono::milliseconds(500), [this]() {return m_pollingMode.load(std::memory_order_relaxed) != polling_mode::wait || m_done.load(std::memory_order_relaxed); });
						m_active.fetch_add(1, std::memory_order_relaxed);
					}
				}
				return;
			}
		}

		bool poll(unsigned int tIndex, function_wrapper& task)
		{
			return pop_local_queue_task(tIndex, task) || pop_task_from_pool_queue(task) || try_steal(task);
		}

		bool check_suspend_polling(unsigned int tIndex, function_wrapper& task)
		{
			//! I'm assuming no further tasks are submitted when this is called. This is a hard precondition.
			if (m_suspendPolling.load(std::memory_order_relaxed))
			{
				m_active.fetch_sub(1, std::memory_order_relaxed);
				{
					auto lk = std::unique_lock<mutex_type>{ m_suspendMtx };
					m_suspendCdn.wait(lk, [this]() { return !m_suspendPolling.load(std::memory_order_relaxed) || m_done.load(std::memory_order_relaxed); });
				}
				m_active.fetch_add(1, std::memory_order_relaxed);
				return true;
			}

			return false;
		}

		bool pop_local_queue_task(unsigned int i, function_wrapper& task)
		{
			return queue_traits::try_pop(*m_localQs[i], task);
		}

		bool pop_task_from_pool_queue(function_wrapper& task)
		{
			return queue_traits::try_pop(m_poolQ, task);
		}

		bool try_steal(function_wrapper& task)
		{
			for (unsigned int i = 0; i < m_localQs.size(); ++i)
			{
				if (queue_traits::try_steal(*m_localQs[i], task))
					return true;
			}

			return false;
		}

	public:

		template <typename T>
		using future = typename thread_traits::template future_type<T>;

		work_stealing_thread_pool(unsigned int nthreads = boost::thread::hardware_concurrency())
			: m_threads(nthreads)
			, m_done(false)
			, m_workQIndex([](){ return static_cast<std::uint32_t>(-1); })
			, m_localQs(nthreads)
		{
			init();
		}

		work_stealing_thread_pool(std::function<void()> onThreadStart, std::function<void()> onThreadStop, unsigned int nthreads = boost::thread::hardware_concurrency())
			: m_threads(nthreads)
			, m_done(false)
			, m_workQIndex([](){ return static_cast<std::uint32_t>(-1); })
			, m_localQs(nthreads)
			, m_onThreadStart(onThreadStart)
			, m_onThreadStop(onThreadStop)
		{
			init();
		}

		~work_stealing_thread_pool()
		{
			m_done.store(true, std::memory_order_relaxed);
			if (m_suspendPolling.load(std::memory_order_relaxed))
				resume_polling();
			else if (m_pollingMode.load(std::memory_order_relaxed) == polling_mode::wait)
			{
				while (m_active.load(std::memory_order_relaxed) != number_threads())
					m_pollingCnd.notify_all();
			}
			boost::for_each(m_threads, [](thread_type& t)
			{
				if (t.joinable())
					t.join();
			});
		}

		template <typename Action>
		future<decltype(std::declval<Action>()())> send(Action&& x)
		{
			return send_impl(std::forward<Action>(x));
		}

		template <typename Action>
		future<decltype(std::declval<Action>()())> send(std::uint32_t threadIndex, Action&& x)
		{
			GEOMETRIX_ASSERT(threadIndex < number_threads());
			return send_impl(threadIndex, std::forward<Action>(x));
		}

		std::size_t number_threads() const { return m_nThreads.load(std::memory_order_relaxed); }

		//! Suspend polling. This method will put the threads to sleep on a condition until polling is resumed.
		//! The precondition to calling this method is that all futures for submitted tasks have had their wait/get method called.
		//! This blocks until all pool threads block on the condition.
		void suspend_polling()
		{
			GEOMETRIX_ASSERT(m_nTasksOutstanding.load(std::memory_order_relaxed) == 0);
			GEOMETRIX_ASSERT(m_suspendPolling.load(std::memory_order_relaxed) == false);
			auto lk = std::unique_lock<mutex_type>{ m_suspendMtx };
			m_suspendPolling.store(true, std::memory_order_relaxed);
			lk.unlock();
			while (m_active.load(std::memory_order_relaxed) != 0); 
		}

		//! This restarts the threadpool. It blocks until all pool threads are no longer blocking on the condition.
		void resume_polling()
		{	
			GEOMETRIX_ASSERT(m_suspendPolling.load(std::memory_order_relaxed) == true);
			{
				auto lk = std::unique_lock<mutex_type>{ m_suspendMtx };
				m_suspendPolling.store(false, std::memory_order_relaxed);
			}
			while (m_active.load(std::memory_order_relaxed) != number_threads()) 
				m_suspendCdn.notify_all();
		}

		void set_polling_mode(polling_mode m)
		{
			auto lk = std::unique_lock<mutex_type>{ m_pollingMtx };
			m_pollingMode.store(m, std::memory_order_relaxed);
		}

	private:

		void init()
		{
			try
			{
				for (unsigned int i = 0; i < m_threads.size(); ++i)
					m_localQs[i] = queue_ptr(new queue_type());

				for (unsigned int i = 0; i < m_threads.size(); ++i)
					m_threads[i] = thread_type([i, this]() { worker_thread(i); });
			}
			catch (...)
			{
				m_done = true;
				throw;
			}
		}

		template <typename Action>
		future<decltype(std::declval<Action>()())> send_impl(Action&& m)
		{
			GEOMETRIX_ASSERT(m_suspendPolling.load(std::memory_order_relaxed) == false);
			GEOMETRIX_ASSERT(m_done.load(std::memory_order_relaxed) == false);
			using result_type = decltype(m());
			packaged_task<result_type> task([m = std::forward<Action>(m), this]()->result_type{ STK_SCOPE_EXIT(m_nTasksOutstanding.fetch_sub(1, std::memory_order_relaxed); ); return m(); });
			auto result = task.get_future();

			auto localIndex = *m_workQIndex;
			m_nTasksOutstanding.fetch_add(1, std::memory_order_relaxed);
			if (localIndex != static_cast<std::uint32_t>(-1))
				queue_traits::push(*m_localQs[localIndex], function_wrapper(std::move(task)));
			else
				queue_traits::push(m_poolQ, function_wrapper(std::move(task)));
			
			if (m_pollingMode.load(std::memory_order_relaxed) == polling_mode::wait)
				m_pollingCnd.notify_one();

			return std::move(result);
		}
		
		template <typename Action>
		future<decltype(std::declval<Action>()())> send_impl(std::uint32_t threadIndex, Action&& m)
		{
			GEOMETRIX_ASSERT(m_suspendPolling.load(std::memory_order_relaxed) == false);
			GEOMETRIX_ASSERT(m_done.load(std::memory_order_relaxed) == false);
			using result_type = decltype(m());
			packaged_task<result_type> task([m = std::forward<Action>(m), this]()->result_type{ STK_SCOPE_EXIT(m_nTasksOutstanding.fetch_sub(1, std::memory_order_relaxed); ); return m(); });
			auto result = task.get_future();

			m_nTasksOutstanding.fetch_add(1, std::memory_order_relaxed);
			if (threadIndex != static_cast<std::uint32_t>(-1))
				queue_traits::push(*m_localQs[threadIndex], function_wrapper(std::move(task)));
			else
				queue_traits::push(m_poolQ, function_wrapper(std::move(task)));
			
			if (m_pollingMode.load(std::memory_order_relaxed) == polling_mode::wait)
				m_pollingCnd.notify_one();

			return std::move(result);
		}

		std::vector<thread_type>       m_threads;
		std::atomic<bool>              m_done;
		thread_specific<std::uint32_t> m_workQIndex;
		queue_type					   m_poolQ;
		std::vector<queue_ptr>	       m_localQs;
		std::function<void()>          m_onThreadStart;
		std::function<void()>          m_onThreadStop;
		std::atomic<std::uint32_t>     m_active{ 0 };
		std::atomic<std::uint32_t>     m_nTasksOutstanding{ 0 };
		std::atomic<std::uint32_t>     m_nThreads{ 0 };
		std::atomic<bool>              m_suspendPolling{ false };
		mutex_type                     m_suspendMtx;
		std::condition_variable_any    m_suspendCdn;
		std::atomic<polling_mode>      m_pollingMode{ polling_mode::fast };
		mutex_type                     m_pollingMtx;
		std::condition_variable_any    m_pollingCnd;
	};

}}//! namespace stk::thread;

#endif // STK_THREAD_WORK_STEALING_THREAD_POOL_HPP

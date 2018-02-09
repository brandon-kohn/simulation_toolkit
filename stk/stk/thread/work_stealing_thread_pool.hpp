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

#include <stk/thread/function_wrapper_with_allocator.hpp>
#include <stk/thread/barrier.hpp>
#include <stk/thread/thread_specific.hpp>
#include <stk/container/locked_queue.hpp>
#include <stk/thread/partition_work.hpp>
#include <stk/thread/boost_thread_kernel.hpp>
#include <stk/utility/scope_exit.hpp>
#include <stk/thread/pool_polling_mode.hpp>
#ifdef STK_USE_JEMALLOC
#include <stk/utility/jemallocator.hpp>
#endif
#include <boost/noncopyable.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <algorithm>
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
#ifdef STK_USE_JEMALLOC
		using alloc_type = stk::jemallocator<char>;
#else
		using alloc_type = std::allocator<char>;
#endif
		using fun_wrapper = function_wrapper_with_allocator<alloc_type>;
		using queue_type = typename queue_traits::template queue_type<fun_wrapper*, std::allocator<fun_wrapper>, mutex_type>;
		using queue_ptr = std::unique_ptr<queue_type>;
		using unique_atomic_bool = std::unique_ptr<std::atomic<bool>>;
	    
		static stk::detail::random_xor_shift_generator create_generator()
	    {
	        std::random_device rd;
	        return stk::detail::random_xor_shift_generator(rd());
	    }

		static unsigned int get_rnd()
		{
			static auto rnd = create_generator();
			return rnd();
		}

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
			fun_wrapper* task;
			std::int32_t spincount = 0;
		polling_mode_fast:
			{
				while (!m_done[tIndex]->load(std::memory_order_relaxed))
				{
					if (poll(tIndex, task))
					{
                        std::unique_ptr<fun_wrapper> d(task);
						(*task)();
						spincount = 0;
					}
					else if (check_suspend_polling(tIndex));
					else if (m_pollingMode.load(std::memory_order_relaxed) == polling_mode::wait)
						goto polling_mode_wait;
					else
					{
						++spincount;
						auto backoff = (std::min)(spincount * 10, 100);
						while(backoff)
						{
							thread_traits::yield();
							--backoff;
						}
					}
				}
				return;
			}
		polling_mode_wait:
			{
				while (!m_done[tIndex]->load(std::memory_order_relaxed))
				{
					if (poll(tIndex, task))
					{
						std::unique_ptr<fun_wrapper> d(task);
						(*task)();
						spincount = 0;
					}
					else if (check_suspend_polling(tIndex));
					else if (m_pollingMode.load(std::memory_order_relaxed) == polling_mode::fast)
						goto polling_mode_fast;
					else
					{
						++spincount;
						if (spincount < 100)
						{
							auto backoff = (std::min)(spincount * 10, 100);
							while (backoff)
							{
								thread_traits::yield();
								--backoff;
							}
						}
						else
						{
							m_active.fetch_sub(1, std::memory_order_relaxed);
							auto lk = std::unique_lock<mutex_type>{ m_pollingMtx };
							m_pollingCnd.wait_for(lk, std::chrono::milliseconds(500), [tIndex, this]() {return m_pollingMode.load(std::memory_order_relaxed) != polling_mode::wait || m_done[tIndex]->load(std::memory_order_relaxed); });
							m_active.fetch_add(1, std::memory_order_relaxed);
						}
					}
				}
				return;
			}
		}

		bool poll(unsigned int tIndex, fun_wrapper*& task)
		{
			return pop_local_queue_task(tIndex, task) || pop_task_from_pool_queue(task) || try_steal(task);
		}
		
		bool check_suspend_polling(unsigned int tIndex)
		{
			//! I'm assuming no further tasks are submitted when this is called. This is a hard precondition.
			if (m_suspendPolling.load(std::memory_order_relaxed))
			{
				m_active.fetch_sub(1, std::memory_order_relaxed);
				{
					auto lk = std::unique_lock<mutex_type>{ m_suspendMtx };
					m_suspendCdn.wait(lk, [tIndex, this]() { return !m_suspendPolling.load(std::memory_order_relaxed) || m_done[tIndex]->load(std::memory_order_relaxed); });
				}
				m_active.fetch_add(1, std::memory_order_relaxed);
				return true;
			}

			return false;
		}

		bool pop_local_queue_task(unsigned int i, fun_wrapper*& task)
		{
			return queue_traits::try_pop(*m_localQs[i], task);
		}

		bool pop_task_from_pool_queue(fun_wrapper*& task)
		{
			return queue_traits::try_steal(m_poolQ, task);
		}

		bool try_steal_all(fun_wrapper*& task)
		{
			for (unsigned int i = 0; i < m_localQs.size(); ++i)
			{
				if (queue_traits::try_steal(*m_localQs[i], task))
					return true;
			}

			return false;
		}

		bool try_steal(fun_wrapper*& task)
		{
			auto victim = get_rnd() % m_localQs.size();
			if (queue_traits::try_steal(*m_localQs[victim], task))
				return true;

			return false;
		}

		void set_done(bool v)
		{
			for (auto& b : m_done)
				b->store(v);
		}

	public:

		template <typename T>
		using future = typename thread_traits::template future_type<T>;

		work_stealing_thread_pool(unsigned int nthreads = boost::thread::hardware_concurrency()-1)
			: m_threads(nthreads)
			, m_workQIndex([](){ return static_cast<std::uint32_t>(-1); })
			, m_poolQ(64*1024)
			, m_localQs(nthreads)
			, m_allocator()
		{
			init();
		}

		work_stealing_thread_pool(std::function<void()> onThreadStart, std::function<void()> onThreadStop, unsigned int nthreads = boost::thread::hardware_concurrency()-1)
			: m_threads(nthreads)
			, m_workQIndex([](){ return static_cast<std::uint32_t>(-1); })
			, m_poolQ(64*1024)
			, m_localQs(nthreads)
			, m_onThreadStart(onThreadStart)
			, m_onThreadStop(onThreadStop)
			, m_allocator()
		{
			init();
		}

		~work_stealing_thread_pool()
		{
			set_done(true);
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

            fun_wrapper* task;
            while(pop_task_from_pool_queue(task) || try_steal_all(task))
                delete task;
		}

		template <typename Action>
		future<decltype(std::declval<Action>()())> send(Action&& x)
		{
			return send_impl(*m_workQIndex, std::forward<Action>(x));
		}

		template <typename Action>
		future<decltype(std::declval<Action>()())> send(std::uint32_t threadIndex, Action&& x)
		{
			GEOMETRIX_ASSERT(threadIndex < number_threads());
			return send_impl(threadIndex, std::forward<Action>(x));
		}

		template <typename Action>
		void send_no_future(Action&& m)
		{
			GEOMETRIX_ASSERT(m_suspendPolling.load(std::memory_order_relaxed) == false);
			GEOMETRIX_ASSERT(m_done.load(std::memory_order_relaxed) == false);

			auto localIndex = *m_workQIndex;
			auto p = new fun_wrapper(m_allocator, std::forward<Action>(m));
			if (localIndex != static_cast<std::uint32_t>(-1))
			{
				while (!queue_traits::try_push(*m_localQs[localIndex], p))
					thread_traits::yield();
			}
			else
			{
				while (!queue_traits::try_push(m_poolQ, p))
					thread_traits::yield();
			}
			
			if (m_pollingMode.load(std::memory_order_relaxed) == polling_mode::wait)
				m_pollingCnd.notify_one();
		}

		template <typename Range, typename TaskFn>
		void parallel_for(Range&& range, TaskFn&& task)
		{
			auto npartitions = number_threads();
			using iterator_t = typename boost::range_iterator<Range>::type;
			std::atomic<std::size_t> consumed = 0;
			partition_work(range, npartitions,
				[&consumed, &task, this](iterator_t from, iterator_t to) -> void
			{
				send_no_future([&consumed, &task, from, to, this]() -> void
				{
					for (auto i = from; i != to; ++i)
						send_no_future([&consumed, i, task]() -> void {consumed.fetch_add(1, std::memory_order_relaxed); task(*i); });
				});
			});

			auto njobs = boost::size(range);
			wait_for([&consumed, njobs]() { return consumed.load(std::memory_order_relaxed) == njobs; });
		}

		std::size_t number_threads() const { return m_nThreads.load(std::memory_order_relaxed); }

		//! Suspend polling. This method will put the threads to sleep on a condition until polling is resumed.
		//! The precondition to calling this method is that all futures for submitted tasks have had their wait/get method called.
		//! This blocks until all pool threads block on the condition.
		void suspend_polling()
		{
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

		template <typename Pred>
		void wait_for(Pred&& pred)
		{
			fun_wrapper* task;
			while (!pred())
			{
				if (pop_task_from_pool_queue(task) || try_steal(task))
				{
					std::unique_ptr<fun_wrapper> d(task);
					(*task)();
				}
			}
		}

	private:

		void init()
		{
			try
			{
				for (unsigned int i = 0; i < m_threads.size(); ++i)
				{
					m_localQs[i] = queue_ptr(new queue_type(64 * 1024));
					m_done.emplace_back(new std::atomic<bool>{ false });
				}

				for (unsigned int i = 0; i < m_threads.size(); ++i)
					m_threads[i] = thread_type([i, this]() { worker_thread(i); });

				while (number_threads() != m_threads.size())
					thread_traits::yield();
			}
			catch (...)
			{
				set_done(true);
				throw;
			}
		}

		template <typename Action>
		future<decltype(std::declval<Action>()())> send_impl(std::uint32_t threadIndex, Action&& m)
		{
			GEOMETRIX_ASSERT(m_suspendPolling.load(std::memory_order_relaxed) == false);
			GEOMETRIX_ASSERT(m_done.load(std::memory_order_relaxed) == false);
			using result_type = decltype(m());
			packaged_task<result_type> task(std::forward<Action>(m));
			auto result = task.get_future();

			auto p = new fun_wrapper(m_allocator, std::move(task));
			if (threadIndex != static_cast<std::uint32_t>(-1))
			{
				while (!queue_traits::try_push(*m_localQs[threadIndex], p))
					thread_traits::yield();
			}
			else
			{
				while (!queue_traits::try_push(m_poolQ, p))
					thread_traits::yield();
			}

			if (m_pollingMode.load(std::memory_order_relaxed) == polling_mode::wait)
				m_pollingCnd.notify_one();

			return std::move(result);
		}

		std::vector<thread_type>       m_threads;
		std::vector<unique_atomic_bool>m_done;
		thread_specific<std::uint32_t> m_workQIndex;
		queue_type					   m_poolQ;
		std::vector<queue_ptr>	       m_localQs;
		std::function<void()>          m_onThreadStart;
		std::function<void()>          m_onThreadStop;
		std::atomic<std::uint32_t>     m_active{ 0 };
		std::atomic<std::uint32_t>     m_nThreads{ 0 };
		std::atomic<bool>              m_suspendPolling{ false };
		mutex_type                     m_suspendMtx;
		std::condition_variable_any    m_suspendCdn;
		std::atomic<polling_mode>      m_pollingMode{ polling_mode::fast };
		mutex_type                     m_pollingMtx;
		std::condition_variable_any    m_pollingCnd;
		alloc_type                     m_allocator;
	};

}}//! namespace stk::thread;

#endif // STK_THREAD_WORK_STEALING_THREAD_POOL_HPP

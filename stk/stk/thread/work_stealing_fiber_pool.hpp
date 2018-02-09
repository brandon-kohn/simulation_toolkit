//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_THREAD_WORKSTEALING_FIBER_POOL_HPP
#define STK_THREAD_WORKSTEALING_FIBER_POOL_HPP
#pragma once

#include <stk/thread/function_wrapper.hpp>
#include <stk/thread/barrier.hpp>
#include <stk/thread/thread_specific.hpp>
#include <stk/container/locked_queue.hpp>
#include <stk/thread/atomic_spin_lock.hpp>
#include <stk/thread/boost_thread_kernel.hpp>
#include <stk/utility/scope_exit.hpp>
#include <stk/thread/pool_polling_mode.hpp>

#include <boost/noncopyable.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/fiber/all.hpp>
#include <cstdint>

#ifndef STK_NO_FIBER_POOL_BIND_TO_PROCESSOR
#include <stk/thread/bind/bind_processor.hpp>
#endif

namespace stk { namespace thread {

template <typename Allocator = boost::fibers::fixedsize_stack, typename QTraits = locked_queue_traits, typename ThreadTraits = boost_thread_traits>
class work_stealing_fiber_pool : boost::noncopyable
{
    using thread_traits = ThreadTraits;
	using queue_traits = QTraits;
    using allocator_type = Allocator;

    using thread_type = typename thread_traits::thread_type;
    using fiber_type = boost::fibers::fiber;

    template <typename T>
    using packaged_task = boost::fibers::packaged_task<T>;

    using thread_mutex_type = typename thread_traits::mutex_type;
	using thread_lock_type = std::unique_lock<thread_mutex_type>;
	
	using fiber_mutex_type = boost::fibers::mutex;
	using fiber_lock_type = std::unique_lock<fiber_mutex_type>;

    using queue_type = typename queue_traits::template queue_type<function_wrapper*, std::allocator<function_wrapper*>, fiber_mutex_type>;
	using queue_ptr = std::unique_ptr<queue_type>;

    void os_thread(std::size_t nThreads, std::size_t nFibersPerThread, unsigned int idx)
    {
		if (m_onThreadStart)
			m_onThreadStart();

		STK_SCOPE_EXIT(
			if (m_onThreadStop)
				m_onThreadStop();
		);
#ifndef STK_NO_FIBER_POOL_BIND_TO_PROCESSOR
		bind_to_processor(idx % std::thread::hardware_concurrency());
#endif
		boost::fibers::use_scheduling_algorithm<boost::fibers::algo::round_robin>();
        m_barrier.wait();
		
		auto start = idx * nFibersPerThread;
		auto end = start + nFibersPerThread;
		for (std::size_t i = start; i < end; ++i)
			m_fibers[i] = boost::fibers::fiber(boost::fibers::launch::post, std::allocator_arg, m_alloc, [idx, this]() { worker_fiber(idx); });
		
		{
			fiber_lock_type lk{ m_fiberMtx };
			//! Wake up every second or so to check if a notification was missed.
			while(!m_shutdownCondition.wait_for(lk, std::chrono::seconds(1), [this]() { return m_done.load(std::memory_order_relaxed); }));
		}
        GEOMETRIX_ASSERT(m_done.load(std::memory_order_relaxed));
		
		for (std::size_t i = start; i < end; ++i)
		{
			try
			{
				if (m_fibers[i].joinable())
					m_fibers[i].join();
			} 
			catch(...)
			{}
		}
    }

	void worker_fiber(unsigned int tid)
	{
		m_nFibers.fetch_add(1, std::memory_order_relaxed);
		m_active.fetch_add(1, std::memory_order_relaxed);

		STK_SCOPE_EXIT(
			m_active.fetch_sub(1, std::memory_order_relaxed);
			m_nFibers.fetch_sub(1, std::memory_order_relaxed);
		);

		m_workQIndex = tid;
		function_wrapper* task;
		while (!m_done.load(std::memory_order_relaxed))
		{
			if (poll(tid, task))
			{
				std::unique_ptr<function_wrapper> d(task);
				(*task)();
			}
			else if (check_suspend_polling());
			boost::this_fiber::yield();
		}
	}

	bool poll(unsigned int tIndex, function_wrapper*& task)
	{
		return pop_local_queue_task(tIndex, task) || pop_task_from_pool_queue(task) || try_steal(task);
	}

	bool check_suspend_polling()
	{
		//! I'm assuming no further tasks are submitted when this is called. This is a hard precondition.
		if (m_suspendPolling.load(std::memory_order_relaxed))
		{
			m_active.fetch_sub(1, std::memory_order_relaxed);
			{
				auto lk = std::unique_lock<fiber_mutex_type>{ m_suspendMtx };
				m_suspendCdn.wait(lk, [this]() { return !m_suspendPolling.load(std::memory_order_relaxed); });
			}
			m_active.fetch_add(1, std::memory_order_relaxed);
			return true;
		}

		return false;
	}

	bool pop_local_queue_task(unsigned int i, function_wrapper*& task)
	{
		return queue_traits::try_pop(*m_localQs[i], task);
	}

	bool pop_task_from_pool_queue(function_wrapper*& task)
	{
		return queue_traits::try_pop(m_poolQ, task);
	}

	bool try_steal(function_wrapper*& task)
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
    using future = boost::fibers::future<T>;

    work_stealing_fiber_pool(std::size_t nFibersPerThread, const allocator_type& alloc, unsigned int nOSThreads = std::thread::hardware_concurrency() - 1)
		: m_done(false)
		, m_workQIndex([]() { return static_cast<std::uint32_t>(-1); })
		, m_localQs(nOSThreads)
		, m_alloc(alloc)
		, m_barrier(nOSThreads + 1)
    {
		init(nOSThreads, nFibersPerThread);
    }

	work_stealing_fiber_pool(std::function<void()> onThreadStart, std::function<void()> onThreadStop, std::size_t nFibersPerThread, const allocator_type& alloc, unsigned int nOSThreads = std::thread::hardware_concurrency() - 1)
		: m_done(false)
		, m_workQIndex([]() { return static_cast<std::uint32_t>(-1); })
		, m_localQs(nOSThreads)
		, m_alloc(alloc)
		, m_barrier(nOSThreads + 1)
		, m_onThreadStart(onThreadStart)
		, m_onThreadStop(onThreadStop)
	{
		init(nOSThreads, nFibersPerThread);
	}

    ~work_stealing_fiber_pool()
    {
        shutdown();
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
		
	std::size_t number_threads() const { return m_threads.size(); }
	std::size_t number_fibers() const { return m_fibers.size(); }

	//! Suspend polling. This method will put the threads to sleep on a condition until polling is resumed.
	//! The precondition to calling this method is that all futures for submitted tasks have had their wait/get method called.
	//! This blocks until all pool threads block on the condition.
	void suspend_polling()
	{
		GEOMETRIX_ASSERT(m_nTasksOutstanding.load(std::memory_order_relaxed) == 0);
		GEOMETRIX_ASSERT(m_suspendPolling.load(std::memory_order_relaxed) == false);
		auto lk = std::unique_lock<fiber_mutex_type>{ m_suspendMtx };
		m_suspendPolling.store(true, std::memory_order_relaxed);
		lk.unlock();
		while (m_active.load(std::memory_order_relaxed) != 0) boost::this_fiber::yield(); 
	}
	
	//! This restarts the threadpool. It blocks until all pool threads are no longer blocking on the condition.
	void resume_polling()
	{	
		GEOMETRIX_ASSERT(m_suspendPolling.load(std::memory_order_relaxed) == true);
		{
			auto lk = std::unique_lock<fiber_mutex_type>{ m_suspendMtx };
			m_suspendPolling.store(false, std::memory_order_relaxed);
		}
		while (m_active.load(std::memory_order_relaxed) != number_fibers())
		{
			m_suspendCdn.notify_all();
			boost::this_fiber::yield();
		}
	}

private:

	void init(std::uint32_t nOSThreads, std::size_t nFibersPerThread)
	{
		GEOMETRIX_ASSERT(nOSThreads > 1);//1 thread? why?
		if (nOSThreads < 2)
			throw std::invalid_argument("fiber pool should have at least 2 OS threads.");

		try
		{
			auto nCPUS = std::thread::hardware_concurrency();
			m_threads.reserve(nOSThreads);
			m_fibers.resize(nFibersPerThread * nOSThreads);

			for (std::size_t i = 0; i < nOSThreads; ++i)
				m_localQs[i] = queue_ptr(new queue_type());

			for (unsigned int i = 0; i < nOSThreads; ++i)
				m_threads.emplace_back([this](std::size_t nThreads, std::size_t nFibersPerThread, unsigned int idx) { os_thread(nThreads, nFibersPerThread, idx); }, nOSThreads, nFibersPerThread, i);
			m_barrier.wait();//! synchronize just before fiber creation.
		}
		catch (...)
		{
			shutdown();
			throw;
		}
	}

    template <typename Action>
    future<decltype(std::declval<Action>()())> send_impl(Action&& m)
    {
        using result_type = decltype(m());
		packaged_task<result_type()> task([m = std::forward<Action>(m), this]()->result_type{ STK_SCOPE_EXIT(m_nTasksOutstanding.fetch_sub(1, std::memory_order_relaxed); ); return m(); });
        auto result = task.get_future();

		auto localIndex = *m_workQIndex;
		m_nTasksOutstanding.fetch_add(1, std::memory_order_relaxed);
		auto pFn = new function_wrapper(std::move(task));
		if (localIndex != static_cast<std::uint32_t>(-1))
		{
			while (!queue_traits::try_push(*m_localQs[localIndex], pFn))
				boost::this_fiber::yield();
		}
		else
		{
			while (!queue_traits::try_push(m_poolQ, pFn))
				boost::this_fiber::yield();
		}

        return std::move(result);
    } 

	template <typename Action>
	future<decltype(std::declval<Action>()())> send_impl(std::uint32_t threadIndex, Action&& m)
	{
		using result_type = decltype(m());
		packaged_task<result_type()> task([m = std::forward<Action>(m), this]()->result_type{ STK_SCOPE_EXIT(m_nTasksOutstanding.fetch_sub(1, std::memory_order_relaxed); ); return m(); });
		auto result = task.get_future();

		m_nTasksOutstanding.fetch_add(1, std::memory_order_relaxed);
		auto pFn = new function_wrapper(std::move(task));
		if (threadIndex != static_cast<std::uint32_t>(-1))
		{
			while (!queue_traits::try_push(*m_localQs[threadIndex], pFn))
				boost::this_fiber::yield();
		}
		else
		{
			while (!queue_traits::try_push(m_poolQ, pFn))
				boost::this_fiber::yield();
		}

		return std::move(result);
	}

    void shutdown()
    {
		fiber_lock_type lk{ m_fiberMtx };
		m_done.store(true, std::memory_order_relaxed);
		lk.unlock();
		m_shutdownCondition.notify_all();
			
        boost::for_each(m_threads, [](thread_type& t)
        {
            try
            {
                if (t.joinable())
                    t.join();
            }
            catch (...)
            {}
        });
    }

    std::vector<thread_type>                m_threads;
    std::vector<fiber_type>                 m_fibers;
    std::atomic<bool>                       m_done;
	thread_specific<std::uint32_t>          m_workQIndex;
	queue_type								m_poolQ;
	std::vector<queue_ptr>					m_localQs;
	fiber_mutex_type						m_fiberMtx;
    boost::fibers::condition_variable_any	m_shutdownCondition;
    allocator_type							m_alloc;
	barrier<thread_mutex_type>				m_barrier;
	std::function<void()>					m_onThreadStart;
	std::function<void()>					m_onThreadStop;
	std::atomic<std::uint32_t>				m_active{ 0 };
	std::atomic<std::uint32_t>				m_nTasksOutstanding{ 0 };
	std::atomic<std::uint32_t>				m_nFibers{ 0 };
	std::atomic<bool>						m_suspendPolling{ false };
	fiber_mutex_type				        m_suspendMtx;
	boost::fibers::condition_variable_any   m_suspendCdn;
	std::atomic<polling_mode>				m_pollingMode{ polling_mode::fast };
};

}}//! namespace stk::thread;

#endif // STK_THREAD_WORKSTEALING_FIBER_POOL_HPP

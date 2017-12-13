
#ifndef STK_THREAD_M_N_THREADPOOL_HPP
#define STK_THREAD_M_N_THREADPOOL_HPP
#pragma once

#include <stk/thread/function_wrapper.hpp>
#include <stk/thread/barrier.hpp>
#include <stk/container/locked_queue.hpp>
#include <stk/thread/atomic_spin_lock.hpp>
#include <stk/thread/boost_thread_kernel.hpp>
#include <boost/noncopyable.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/fiber/all.hpp>
#include <cstdint>

#ifndef STK_NO_FIBER_POOL_BIND_TO_PROCESSOR
#include <stk/thread/bind/bind_processor.hpp>
#endif

namespace stk { namespace thread {

template <typename Allocator = boost::fibers::fixedsize_stack, typename Traits = boost_thread_traits>
class fiber_pool : boost::noncopyable
{
    using thread_traits = Traits;
    using allocator_type = Allocator;

    using thread_type = typename thread_traits::thread_type;
    using fiber_type = boost::fibers::fiber;

    template <typename T>
    using packaged_task = boost::fibers::packaged_task<T>;

    using mutex_type = typename thread_traits::mutex_type;
	using fiber_mutex_type = atomic_spin_lock</*fiber_yield_wait*/>;// boost::fibers::mutex;

    using queue_type = locked_queue<function_wrapper, std::allocator<function_wrapper>, fiber_mutex_type>;

    void os_thread(std::size_t nThreads, std::size_t nFibersPerThread, unsigned int idx)
    {
#ifndef STK_NO_FIBER_POOL_BIND_TO_PROCESSOR
        bind_to_processor(idx);
#endif
        //boost::fibers::use_scheduling_algorithm<boost::fibers::algo::shared_work>(/*nThreads*/);
        m_barrier.wait();

		auto start = idx * nFibersPerThread;
		auto end = start + nFibersPerThread;
		for (std::size_t i = start; i < end; ++i)
			m_fibers[i] = boost::fibers::fiber(boost::fibers::launch::dispatch, std::allocator_arg, m_alloc, [this]() { worker_fiber(); });
		
		lock_type lk{ m_mtx };
		m_cnd.wait(lk, [this]() { return m_done.load() > 1; });
        GEOMETRIX_ASSERT(m_done);
    }

    void worker_fiber()
    {
		function_wrapper task;
		while (!m_done.load())
		{
			if (m_tasks.try_pop(task))
				task();
			boost::this_fiber::yield();
		}
    }
	
public:

    template <typename T>
    using future = boost::fibers::future<T>;

    fiber_pool(std::size_t nFibersPerThread, const allocator_type& alloc, unsigned int nOSThreads = std::thread::hardware_concurrency() - 1)
        : m_done(0)
        , m_alloc(alloc)
		, m_barrier(nOSThreads + 1)
    {
        GEOMETRIX_ASSERT(nOSThreads > 1);//1 thread? why?
		if (nOSThreads < 2)
			throw std::invalid_argument("fiber pool should have at least 2 OS threads.");
		
        try
        {
            auto nCPUS = std::thread::hardware_concurrency();
            m_threads.reserve(nOSThreads);
			m_fibers.resize(nFibersPerThread * nOSThreads);

            for(unsigned int i = 0; i < nOSThreads; ++i)
                m_threads.emplace_back([this](std::size_t nThreads, std::size_t nFibersPerThread, unsigned int idx){ os_thread(nThreads, nFibersPerThread, idx); }, nOSThreads, nFibersPerThread, i % nCPUS);
            m_barrier.wait();
        }
        catch(...)
        {
            shutdown();
            throw;
        }
    }

    ~fiber_pool()
    {
        shutdown();
    }

    template <typename Action>
    future<decltype(std::declval<Action>()())> send(const Action& x)
    {
        return send_impl(static_cast<const Action&>(x));
    }

    template <typename Action>
    future<decltype(std::declval<Action>()())> send(Action&& x)
    {
        return send_impl(std::forward<Action>(x));
    }

private:

    template <typename Action>
    future<decltype(std::declval<Action>()())> send_impl(Action&& m)
    {
        using result_type = decltype(m());
        packaged_task<result_type()> task(std::forward<Action>(m));
        auto result = task.get_future();

        function_wrapper wrapped(std::move(task));
        m_tasks.push_or_wait(std::move(wrapped));

        return std::move(result);
    }

    void shutdown()
    {
        int b = 0;
        if( m_done.compare_exchange_strong(b, 1) )
        {
            //! done=1 shutdown the fibers.
            boost::for_each(m_fibers, [](fiber_type& t)
            {
                try
                {
                    if (t.joinable())
                        t.join();
                } catch(...)
                {}
            });

            m_done = 2;//! shutdown the threads (after the fibers.)
            m_cnd.notify_all();
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
    }

    std::vector<thread_type>                m_threads;
    std::vector<fiber_type>                 m_fibers;
    std::atomic<int>                        m_done;
    queue_type                              m_tasks;

    using lock_type = std::unique_lock<mutex_type>;
    mutex_type                              m_mtx;
    boost::fibers::condition_variable_any   m_cnd;
    allocator_type                          m_alloc;
	barrier<mutex_type>						m_barrier;
};


}}//! namespace stk::thread;

#endif // STK_THREAD_M_N_THREADPOOL_HPP


#ifndef STK_THREAD_WAITABLETHREADPOOL_HPP
#define STK_THREAD_WAITABLETHREADPOOL_HPP
#pragma once

#include <stk/thread/function_wrapper.hpp>
#include <stk/container/locked_queue.hpp>
#include <stk/thread/boost_thread_kernel.hpp>
#include <boost/noncopyable.hpp>
#include <boost/range/algorithm/for_each.hpp>

namespace stk { namespace thread {

template <typename QTraits = locked_queue_traits, typename Traits = boost_thread_traits>
class thread_pool : boost::noncopyable
{
    using thread_traits = Traits;
	using queue_traits = QTraits;

    using thread_type = typename thread_traits::thread_type;
        
    template <typename T>
    using packaged_task = typename Traits::template packaged_task_type<T>;
	
	using mutex_type = typename thread_traits::mutex_type;
	using queue_type = typename queue_traits::template queue_type<function_wrapper, std::allocator<function_wrapper>, mutex_type>;

    void worker_thread()
    {
		if (m_onThreadStart)
			m_onThreadStart();

		function_wrapper task;
		while (!m_done.load(std::memory_order_relaxed))
		{
			if (queue_traits::try_pop(m_tasks, task))
				task();
			else
				thread_traits::yield();
		}

		if (m_onThreadStop)
			m_onThreadStop();
    }
	
public:

    template <typename T>
    using future = typename Traits::template future_type<T>;

    thread_pool(unsigned int nThreads = std::thread::hardware_concurrency())
        : m_done(false)
    {
		init(nThreads);
    }

	thread_pool(std::function<void()> onThreadStart, std::function<void()> onThreadStop, unsigned int nthreads = boost::thread::hardware_concurrency())
		: m_done(false)
		, m_onThreadStart(onThreadStart)
		, m_onThreadStop(onThreadStop)
	{
		init(nthreads);
	}

    ~thread_pool()
    {
        m_done = true;
        boost::for_each(m_threads, [](thread_type& t){ t.join(); });
    }

	template <typename Action>
    future<decltype(std::declval<Action>()())> send(Action&& x)
    {
        return send_impl(std::forward<Action>(x));
    }

	std::size_t number_threads() const { return m_threads.size(); }

private:

	void init(unsigned int nThreads)
	{
		GEOMETRIX_ASSERT(nThreads > 1);//! why 1?
		try
		{
			m_threads.reserve(nThreads);
			for (unsigned int i = 0; i < nThreads; ++i)
				m_threads.emplace_back([this]() { worker_thread(); });
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
        using result_type = decltype(m());
        packaged_task<result_type> task(boost::forward<Action>(m));
        auto result = task.get_future();
		        
        queue_traits::push(m_tasks, function_wrapper(std::move(task)));

        return std::move(result);
    }

    std::atomic<bool>           m_done;
    std::vector<thread_type>    m_threads;
    queue_type					m_tasks;
	std::function<void()>       m_onThreadStart;
	std::function<void()>       m_onThreadStop;
};

}}//! namespace stk::thread;

#endif // STK_THREAD_WAITABLETHREADPOOL_HPP

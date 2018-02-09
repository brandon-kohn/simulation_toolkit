
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
	using queue_type = typename queue_traits::template queue_type<function_wrapper*, std::allocator<function_wrapper>, mutex_type>;

    void worker_thread()
    {
		if (m_onThreadStart)
			m_onThreadStart();

		function_wrapper* task;
        unsigned int spincount = 0;
		bool hasTasks = true;
		while (!m_done.load(std::memory_order_relaxed))
		{
			while (hasTasks)
			{
				hasTasks = queue_traits::try_pop(m_tasks, task);
				if (hasTasks)
				{
					std::unique_ptr<function_wrapper> d(task);
					(*task)();
				}
			}

			if(spincount < 1000)
            {
                ++spincount;
				auto backoff = spincount * 10;
				while(backoff)
				{
					thread_traits::yield();
					--backoff;
				}
			} 
			else
			{
				std::unique_lock<std::mutex> lk{ m_mutex };
				m_cnd.wait(lk, [&task, &hasTasks, this]() {
					return (hasTasks = queue_traits::try_pop(m_tasks, task) || m_done.load(std::memory_order_relaxed));
				});
			}
		}

		if (m_onThreadStop)
			m_onThreadStop();
    }
	
public:

    template <typename T>
    using future = typename Traits::template future_type<T>;

    thread_pool(unsigned int nThreads = std::thread::hardware_concurrency()-1)
        : m_done(false)
    {
		init(nThreads);
    }

	thread_pool(std::function<void()> onThreadStart, std::function<void()> onThreadStop, unsigned int nthreads = boost::thread::hardware_concurrency()-1)
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

	template <typename Pred>
	void wait_for(Pred&& pred)
	{
		function_wrapper* task;
		while (!pred())
		{
			if (queue_traits::try_pop(m_tasks, task))
			{
				std::unique_ptr<function_wrapper> d(task);
				(*task)();
			}
		}
	}

	template <typename Action>
	void send_no_future(Action m)
	{
		auto pFn = new function_wrapper(m);
		while (!queue_traits::try_push(m_tasks, pFn))
			thread_traits::yield();
		m_cnd.notify_one();
	}

	template <typename Range, typename TaskFn>
	void parallel_for(Range&& range, TaskFn task)
	{
		auto npartitions = number_threads();
		using iterator_t = typename boost::range_iterator<Range>::type;
		std::atomic<std::size_t> consumed = 0;
		partition_work(range, npartitions,
			[&consumed, &task, this](iterator_t from, iterator_t to) -> void
		{
			send_no_future([&consumed, task, from, to, this]() -> void
			{
				for (auto i = from; i != to; ++i)
					send_no_future([&consumed, i, task]() -> void {consumed.fetch_add(1, std::memory_order_relaxed); task(*i); });
			});
		});

		auto njobs = boost::size(range);
		wait_for([&consumed, njobs]() { return consumed.load(std::memory_order_relaxed) == njobs; });
	}

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
    future<decltype(std::declval<Action>()())> send_impl(Action m)
    {
        using result_type = decltype(m());
        packaged_task<result_type> task(m);
        auto result = task.get_future();
		auto pFn = new function_wrapper(std::move(task));
		while (!queue_traits::try_push(m_tasks, pFn))
			thread_traits::yield();
		m_cnd.notify_one();
        return std::move(result);
    }

    std::atomic<bool>           m_done;
    std::vector<thread_type>    m_threads;
    queue_type					m_tasks;
	std::function<void()>       m_onThreadStart;
	std::function<void()>       m_onThreadStop;
	std::mutex                  m_mutex;
	std::condition_variable     m_cnd;
};

}}//! namespace stk::thread;

#endif // STK_THREAD_WAITABLETHREADPOOL_HPP

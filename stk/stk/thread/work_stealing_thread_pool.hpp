#ifndef STK_THREAD_WORK_STEALING_THREAD_POOL_HPP
#define STK_THREAD_WORK_STEALING_THREAD_POOL_HPP
#pragma once

#include <stk/thread/function_wrapper.hpp>
#include <stk/thread/thread_specific.hpp>
#include <stk/container/locked_queue.hpp>
#include <stk/thread/boost_thread_kernel.hpp>

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
			m_workQIndex = tIndex;
			function_wrapper task;
			while (!m_done)
			{
				if (pop_local_queue_task(tIndex, task) || pop_task_from_pool_queue(task) || try_steal(task))
					task();
				else
					boost::this_thread::yield();
			}
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
			, m_workQIndex([](){ return std::make_shared<std::uint32_t>(static_cast<std::uint32_t>(-1)); })
			, m_localQs(nthreads)
		{
			try
			{
				for (unsigned int i = 0; i < nthreads; ++i)
				{
					m_localQs[i] = queue_ptr(new queue_type());
				}

				for (unsigned int i = 0; i < nthreads; ++i)
				{
					m_threads.emplace_back([i, this]() { worker_thread(i); });
				}
			}
			catch (...)
			{
				m_done = true;
				throw;
			}
		}

		~work_stealing_thread_pool()
		{
			m_done = true;
			boost::for_each(m_threads, [](thread_type& t)
			{
				if (t.joinable())
					t.join();
			});
		}

		template <typename Action>
		future<decltype(std::declval<Action>()())> send(Action const& x)
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
			packaged_task<result_type> task(boost::forward<Action>(m));
			auto result = task.get_future();

			auto localIndex = *m_workQIndex;
			if (localIndex != static_cast<std::uint32_t>(-1))
				queue_traits::push(*m_localQs[localIndex], function_wrapper(std::move(task)));
			else
				queue_traits::push(m_poolQ, function_wrapper(std::move(task)));

			return std::move(result);
		}

		std::vector<thread_type>       m_threads;
		std::atomic<bool>              m_done;
		thread_specific<std::uint32_t> m_workQIndex;
		queue_type					   m_poolQ;
		std::vector<queue_ptr>	       m_localQs;
	};

}}//namespace stk::thread;

#endif // STK_THREAD_WORK_STEALING_THREAD_POOL_HPP

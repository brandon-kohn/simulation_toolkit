//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_THREAD_ACTIVEOBJECT_HPP
#define STK_THREAD_ACTIVEOBJECT_HPP
#pragma once

#include <stk/thread/function_wrapper.hpp>
#include <stk/container/locked_queue.hpp>
#include <stk/thread/std_thread_kernel.hpp>
#include <stk/thread/boost_thread_kernel.hpp>
#include <atomic>

namespace stk { namespace thread {

    template <typename Traits = boost_thread_traits, typename QTraits = locked_queue_traits>
    class active_object
    {
        using thread_traits = Traits;
        using queue_traits = QTraits;
       
        using thread_type = typename thread_traits::thread_type;
        using mutex_type = typename thread_traits::mutex_type;
        using queue_type = typename queue_traits::template queue_type<function_wrapper, std::allocator<function_wrapper>, mutex_type>; 
		using condition_variable_type = typename Traits::condition_variable_type;

		template <typename T>
		using unique_lock = typename thread_traits::template unique_lock<T>;

        template <typename T>
        using future = typename Traits::template future_type<T>;
        
        template <typename T, typename...Ts>
        using packaged_task = typename Traits::template packaged_task_type<T, Ts...>;

    public:

        active_object()
        {
            try
            {
                m_thread = thread_type([this](){ run(); });
            }
            catch(...)
            {
                shutdown();
                throw;
            }
        }

        template <typename ThreadCreationPolicy>
        active_object(ThreadCreationPolicy&& creator)
        {
            try
            {
                m_thread = creator([this]() { run(); });
            }
            catch (...)
            {
                shutdown();
                throw;
            }
        }

        virtual ~active_object()
        {
            shutdown();
        }

        template <typename Action>
        future<decltype(std::declval<Action>()())> send(Action&& x)
        {
            return send_impl(std::forward<Action>(x));
        }

    private:

        void shutdown()
        {
            m_done.store(true, std::memory_order_relaxed);
			m_cnd.notify_one();
            thread_traits::join(m_thread);
        }

        template <typename Action>
        future<decltype(std::declval<Action>()())> send_impl(Action&& m)
        {
            using result_type = decltype(m());
            
            packaged_task<result_type> task(std::forward<Action>(m));
            future<result_type> result(task.get_future());
            function_wrapper p(std::move(task));
             
            while (!queue_traits::try_push(m_tasks, std::move(p)))
                thread_traits::yield();
			m_cnd.notify_one();
            return std::move( result );
        }

        void run()
        {
			function_wrapper task;
			std::uint32_t spincount = 0;
			bool hasTasks = queue_traits::try_pop(m_tasks, task);
			while (true)
			{
				if (hasTasks)
				{
					task();
					if (BOOST_LIKELY(!m_done.load(std::memory_order_relaxed)))
					{
						spincount = 0;
						hasTasks = queue_traits::try_pop(m_tasks, task);
					}
					else
						return;
				}
				//else if (++spincount < 100)
				//{
				//	auto backoff = spincount * 10;
				//	while (backoff--)
				//		thread_traits::yield();
				//	if (BOOST_LIKELY(!m_done.load(std::memory_order_relaxed)))
				//		hasTasks = queue_traits::try_pop(m_tasks, task);
				//	else
				//		return;
				//}
				else
				{
					{
						unique_lock<mutex_type> lk{ m_mutex };
						m_cnd.wait(lk, [&task, &hasTasks, this]() { return (hasTasks = queue_traits::try_pop(m_tasks, task)) || m_done.load(std::memory_order_relaxed); });
					}
					if (!hasTasks)
						return;
				}
			}
        }

        std::atomic<bool>          m_done{false};
        queue_type                 m_tasks;
        thread_type                m_thread;
		mutex_type                 m_mutex;
		condition_variable_type    m_cnd;

    };

}}//! namespace stk::thread;

#endif // STK_THREAD_ACTIVEOBJECT_HPP

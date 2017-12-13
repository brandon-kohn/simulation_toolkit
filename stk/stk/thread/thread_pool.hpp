
#ifndef STK_THREAD_WAITABLETHREADPOOL_HPP
#define STK_THREAD_WAITABLETHREADPOOL_HPP
#pragma once

#include <stk/thread/function_wrapper.hpp>
#include <stk/container/locked_queue.hpp>
#include <stk/thread/boost_thread_kernel.hpp>
#include <boost/noncopyable.hpp>
#include <boost/range/algorithm/for_each.hpp>

namespace stk { namespace thread {

template <typename Traits = boost_thread_traits>
class thread_pool : boost::noncopyable
{
    using thread_traits = Traits;
    
    using thread_type = typename thread_traits::thread_type;
    using thread_ptr = std::unique_ptr<thread_type>;
        
    template <typename T>
    using packaged_task = typename Traits::template packaged_task_type<T>;

    void worker_thread()
    {
        while(!m_done)
            run_pending_task();
    }

    void run_pending_task() 
    {
        function_wrapper task;
        if(m_tasks.try_pop(task))
            task();
        else
            thread_traits::yield();
    }

public:

    template <typename T>
    using future = typename Traits::template future_type<T>;

    thread_pool(unsigned int nThreads = std::thread::hardware_concurrency())
        : m_done(false)
    {
        GEOMETRIX_ASSERT(nThreads > 1);//! why 1?
        try
        {
            m_threads.reserve(nThreads);
            for(unsigned int i = 0; i < nThreads; ++i)
                m_threads.emplace_back([this](){ worker_thread(); });
        }
        catch(...)
        {
            m_done = true;
            throw;
        }
    }

    ~thread_pool()
    {
        m_done = true;
        boost::for_each(m_threads, [](thread_type& t){ t.join(); });
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

    void send(function_wrapper&& m)
    {
        m_tasks.push_or_wait(std::forward<function_wrapper>(m));
    }

private:

    template <typename Action>
    future<decltype(std::declval<Action>()())> send_impl(Action&& m)
    {
        using result_type = decltype(m());
        packaged_task<result_type> task(boost::forward<Action>(m));
        auto result = task.get_future();

        function_wrapper wrapped(std::move(task));
        m_tasks.push_or_wait(std::move(wrapped));

        return std::move(result);
    }

    std::atomic<bool>               m_done;
    std::vector<thread_type>        m_threads;
    locked_queue<function_wrapper>  m_tasks;

};


}}//! namespace stk::thread;

#endif // STK_THREAD_WAITABLETHREADPOOL_HPP

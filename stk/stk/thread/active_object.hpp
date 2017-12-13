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

namespace detail
{
    struct wake_up
    {
        using result_type = void;

        wake_up() = default;

        void operator()() const { }
    };
}//! namespace detail;

template <typename Traits = boost_thread_traits>
class active_object
{
    using thread_traits = Traits;
    using thread_type = typename thread_traits::thread_type;
        
    template <typename T>
    using future = typename Traits::template future_type<T>;
    
    template <typename T, typename...Ts>
    using packaged_task = typename Traits::template packaged_task_type<T, Ts...>;

public:

    active_object()
        : m_done(false)
    {
        try
        {
            m_thread = thread_type([this](){ run(); });
        }
        catch(...)
        {
            shutdown(false);
            throw;
        }
    }

    ~active_object()
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
        return send_impl(std::move(x));
    }

    void send(function_wrapper&& m)
    {
        m_actionQueue.push_or_wait(std::move(m));
    }

    void shutdown(bool finishActions = true)
    {
        if( !m_done )
        {
            m_done = true;
            send(detail::wake_up());
            if( finishActions )
                thread_traits::join(m_thread);
            else
                thread_traits::interrupt(m_thread);
        }
    }

private:

    template <typename Action>
    future<decltype(std::declval<Action>()())> send_impl(Action&& m)
    {
        using result_type = decltype(m());
        
        packaged_task<result_type> task(std::forward<Action>(m));
        future<result_type> result(task.get_future());

        function_wrapper wrapped(std::move(task));
        m_actionQueue.push_or_wait(std::move(wrapped));

        return std::move( result );
    }

    void run()
    {
        while( !m_done )
        {
            function_wrapper action = m_actionQueue.pop_or_wait();
            action();
            thread_traits::interruption_point();
        }
    }

    //! State used locally and not in thread(no synchronization needed)
    std::atomic<bool>              m_done;
    locked_queue<function_wrapper> m_actionQueue;
    thread_type                    m_thread;

};

}}//! namespace stk::thread;

#endif // STK_THREAD_ACTIVEOBJECT_HPP

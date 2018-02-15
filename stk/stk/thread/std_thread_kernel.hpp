//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_THREAD_STD_THREAD_TRAITS_HPP
#define STK_THREAD_STD_THREAD_TRAITS_HPP

#include <thread>
#include <mutex>
#include <future>

#include <boost/thread/shared_mutex.hpp>

namespace stk { namespace thread {

struct std_thread_traits
{
    using thread_type = std::thread;
    using mutex_type = std::mutex;
    using recursive_mutex_type = std::recursive_mutex;

    using shared_mutex_type = boost::shared_mutex;

    template <typename T>
    using future_type = std::future<T>;

    template <typename T, typename...Ts>
    using packaged_task_type = std::packaged_task<T(Ts...)>;

    template <typename T>
    using promise_type = std::promise<T>;

    using condition_variable_type = std::condition_variable;

	template <typename T>
	using unique_lock = std::unique_lock<T>;
    
    static void interrupt(std::thread&){ }
    static void join(std::thread& t){ t.join();}
    static void interruption_point() {  }
    static void yield() { std::this_thread::yield(); }

    template <typename Duration>
    static void sleep_for(Duration&& d) { std::this_thread::sleep_for(d); }

    template <typename T>
    static bool is_ready(future_type<T>& f)
    {
        return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }
};

}}//! namespace stk::thread;

#endif//! STK_THREAD_STD_THREAD_TRAITS_HPP

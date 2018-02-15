//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_THREAD_BOOST_FIBER_TRAITS_HPP
#define STK_THREAD_BOOST_FIBER_TRAITS_HPP

#include <boost/fiber/all.hpp>

namespace stk { namespace thread {

struct boost_fiber_traits
{
    using thread_type = boost::fibers::fiber;
    using mutex_type = boost::fibers::mutex;
    using recursive_mutex_type = boost::fibers::recursive_mutex;
    
    template <typename T>
    using future_type = boost::fibers::future<T>;
    
    template <typename T, typename...Ts>
    using packaged_task_type = boost::fibers::packaged_task<T(Ts...)>;
    
    template <typename T>
    using promise_type = boost::fibers::promise<T>;

    using condition_variable_type = boost::fibers::condition_variable;

	template <typename T>
	using unique_lock = std::unique_lock<T>;

    static void interrupt(boost::fibers::fiber& ){ }
    static void join(boost::fibers::fiber& t){ t.join();}
    static void interruption_point() { }
    
    static void yield() { boost::this_fiber::yield(); }

    template <typename Duration>
    static void sleep_for(Duration&& d) { boost::this_fiber::sleep_for(d); }

	template <typename T>
    static bool is_ready(const future_type<T>& f)
    {
        return f.wait_for(std::chrono::seconds(0)) == boost::fibers::future_status::ready;
    }
};

template <typename StackAllocator = boost::fibers::fixedsize_stack> 
struct boost_fiber_creation_policy
{
	using stack_allocator_type = StackAllocator;

	boost_fiber_creation_policy(const stack_allocator_type& salloc)
		: salloc(salloc)
	{}

	template <typename Fn, typename... Args>
	boost::fibers::fiber operator()(Fn&& fn, Args&&...args)
	{
		return boost::fibers::fiber(boost::fibers::launch::post, std::allocator_arg, salloc, std::forward<Fn>(fn), std::forward<Args>(args)...);
	}

	stack_allocator_type salloc;

};

}}//! namespace stk::thread;

#endif//! STK_THREAD_BOOST_FIBER_TRAITS_HPP

//
//! Copyright © 2018
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/container/atomic_stampable_ptr.hpp>
#include <stk/thread/concurrentqueue.h>
#include <stk/thread/function_wrapper_with_allocator.hpp>
#include <memory>

namespace stk { namespace thread {

    //! Manages nodes using a ref count embedded in an atomic_stampable_ptr. This means on x86 architectures it can only handle a 16 bit ref count.
    class quiesce_memory_reclaimer
    {
    public:

        using func_t = function_wrapper_with_allocator<std::allocator<char>>;

        using queue_type = moodycamel::ConcurrentQueue<func_t>;
		using queue_ptr = queue_type*;
        using atomic_ptr = atomic_stampable_ptr<queue_type>;
        using stamp_type = typename atomic_ptr::stamp_type;

        quiesce_memory_reclaimer()
        {}

        ~quiesce_memory_reclaimer()
        {
            func_t task;
            while(m_queue.try_dequeue(task))
                task();
        }

		void quiesce()
		{
            func_t task;
            while(m_queue.try_dequeue(task))
                task();
		}

        template <typename Fn>
        void add(Fn&& fn)
        {
            m_queue.enqueue(std::forward<Fn>(fn));
        }
		
		void add_checkout()
		{
		}

		void remove_checkout()
		{
		}

		template <typename T>
        void add_checkout(T*dummy=nullptr)
        {

        }

		template <typename T>
        void remove_checkout(T*dummy=nullptr)
        {
        }

    private:

        queue_type m_queue;

    };
}}//! namespace stk::thread;


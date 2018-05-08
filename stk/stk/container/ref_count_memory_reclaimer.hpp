//
//! Copyright © 2018
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
//! Based on:
//! Multiple improvements were included from studying folly's ConcurrentSkipList:
/*
* Copyright 2017 Facebook, Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
// @author: Xin Liu <xliux@fb.com>
//
#pragma once

#include <stk/container/atomic_stampable_ptr.hpp>
#include <stk/thread/concurrentqueue.h>
#include <stk/thread/function_wrapper_with_allocator.hpp>
#include <memory>

namespace stk { namespace thread {

    //! Manages nodes using a ref count embedded in an atomic_stampable_ptr. This means on x86 architectures it can only handle a 16 bit ref count.
    class ref_count_memory_reclaimer
    {
    public:

        using func_t = function_wrapper_with_allocator<std::allocator<char>>;

        using queue_type = moodycamel::ConcurrentQueue<func_t>;
		using queue_ptr = queue_type*;
        using atomic_ptr = atomic_stampable_ptr<queue_type>;
        using stamp_type = typename atomic_ptr::stamp_type;

        ref_count_memory_reclaimer()
            : m_queue(new queue_type())
        {}

        ~ref_count_memory_reclaimer()
        {
			queue_ptr p;
            stamp_type s;
            std::tie(p,s) = m_queue.load(std::memory_order_acquire);

            //! There should not be anything checked out at this point.
            GEOMETRIX_ASSERT(s == 0);
            std::unique_ptr<queue_type> pQueue(p);
            if (pQueue)
            {
                func_t task;
                while(pQueue->try_dequeue(task))
                    task();
            }
        }

		void quiesce()
		{
			queue_ptr p;
            stamp_type s;
            std::tie(p,s) = m_queue.load(std::memory_order_acquire);

            //! There should not be anything checked out at this point.
            GEOMETRIX_ASSERT(s == 0);
            if (p)
            {
                func_t task;
                while(p->try_dequeue(task))
                    task();
            }
		}

        template <typename Fn>
        void add(Fn&& fn)
        {
            queue_ptr p;
            stamp_type s;
            std::tie(p,s) = add_checkout();
            STK_MEMBER_SCOPE_EXIT(
                while(!m_queue.compare_exchange_weak(p, s, p, s-1, std::memory_order_release));
			);
            GEOMETRIX_ASSERT(p);
            p->enqueue(std::forward<Fn>(fn));
        }
		
		std::tuple<queue_ptr, stamp_type> add_checkout()
		{
			return add_checkout<void>();
		}

		void remove_checkout()
		{
			remove_checkout<void>();
		}

		template <typename T>
        std::tuple<queue_ptr, stamp_type> add_checkout(T*dummy=nullptr)
        {
            queue_ptr p;
            stamp_type s;
            std::tie(p,s) = m_queue.load(std::memory_order_acquire);
            while(!m_queue.compare_exchange_weak(p, s, p, s+1, std::memory_order_release));
            return std::make_tuple(p,s+1);
        }

		template <typename T>
        void remove_checkout(T*dummy=nullptr)
        {
            queue_ptr p;
            stamp_type s;
            std::tie(p,s) = m_queue.load(std::memory_order_acquire);
            GEOMETRIX_ASSERT(s > 0);
            STK_MEMBER_SCOPE_EXIT(while(!m_queue.compare_exchange_weak(p, s, p, s-1, std::memory_order_release)));

            if (BOOST_UNLIKELY(s == 1))
            {
                auto toDelete = boost::make_unique<queue_type>();
                if(m_queue.compare_exchange_strong(p, s, toDelete.get(), s, std::memory_order_release))
                {
                    toDelete.release();
                    toDelete.reset(p);
                    func_t task;
                    while(toDelete->try_dequeue(task))
                        task();
                }
            }
        }

    private:

        atomic_ptr     m_queue;

    };
}}//! namespace stk::thread;


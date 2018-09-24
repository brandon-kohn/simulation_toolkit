//
//! Copyright © 2018
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <memory>
#include <mutex>
#include <stk/container/experimental/memory_reclamation_traits.hpp>
#include <stk/thread/function_wrapper_with_allocator.hpp>

namespace stk { 
    namespace thread {

        class simple_qsbr
        {
            using fun_t = stk::thread::function_wrapper_with_allocator<std::allocator<char>>;
        public:


            simple_qsbr() = default;

            ~simple_qsbr()
            {
                release();
            }

            template <typename Fn>
            void add(Fn&& fn)
            {
                auto lk = std::unique_lock<std::mutex>{ m_mtx };
                m_current.emplace_back(std::forward<Fn>(fn));
            }

            void release()
            {
                exec(m_current);
            }

        private:


            void exec(std::vector<fun_t>& fns)
            {
                std::vector<fun_t> toRun;
                {
                    auto lk = std::unique_lock<std::mutex>{ m_mtx };
                    toRun.swap(fns);
                }
                for(auto& fn : toRun)
                    fn();
            }

            std::mutex            m_mtx;
            std::vector<fun_t>    m_current;

        };
    }//! namespace stk::thread;

    template <>
    struct memory_reclamation_traits<stk::thread::simple_qsbr>
    {
        template <typename U, typename Alloc>
        static void reclaim_from_allocator(stk::thread::simple_qsbr& reclaimer, Alloc& alloc, U* ptr, std::size_t n)
        {
            reclaimer.add([=]() mutable -> void
			{
				std::allocator_traits<typename std::decay<Alloc>::type>::destroy(alloc, ptr); 
                std::allocator_traits<typename std::decay<Alloc>::type>::deallocate(alloc, ptr, n);
			});  
        }
    };
}//! namespace stk::thread;


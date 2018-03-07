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
#include <stk/thread/function_wrapper_with_allocator.hpp>

namespace stk { namespace thread {

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
        void register(Fn&& fn)
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
}}//! namespace stk::thread;


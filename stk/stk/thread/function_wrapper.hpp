//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_THREAD_FUNCTIONWRAPPER_HPP
#define STK_THREAD_FUNCTIONWRAPPER_HPP
#pragma once

#include <geometrix/utility/assert.hpp>
#include <memory>

namespace stk { namespace thread {

    //! From Anthony William's Concurrency in Action.
    class function_wrapper
    {
        struct impl_base
        {
        public:
            virtual void call() = 0;
            virtual ~impl_base(){}
        };

        template <typename F>
        struct move_impl_type : impl_base
        {
            move_impl_type(F&& f)
                : m_f(std::move(f))
            {}

            void call()
            {
                m_f();
            }

            F m_f;
            char cachepad[64];
        };

        template <typename F>
        impl_base* make_impl(F&& f)
        {
            return new move_impl_type<F>(std::move(f));
        }

        std::unique_ptr<impl_base> m_pImpl;

    public:

        using result_type = void;

        function_wrapper() = default;

        function_wrapper(const function_wrapper&) = delete;
        function_wrapper& operator=(const function_wrapper&) = delete;

        template <typename F, typename std::enable_if<!std::is_same<F, function_wrapper>::value, int>::type = 0>
        function_wrapper(F&& f)
            : m_pImpl( make_impl(std::forward<F>(f)) )
        {}

        function_wrapper(function_wrapper&& f)
            : m_pImpl(std::move(f.m_pImpl))
        {}

        function_wrapper& operator=(function_wrapper&& x)
        {
            m_pImpl = std::move(x.m_pImpl);
            return *this;
        }

        void operator()()
        {
            GEOMETRIX_ASSERT(m_pImpl);
            m_pImpl->call();
        }

        bool empty() const
        {
            return !m_pImpl;
        }
    };

}}//! namespace stk::thread;

#endif // STK_THREAD_FUNCTIONWRAPPER_HPP

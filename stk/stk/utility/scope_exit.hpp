//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_SCOPEEXIT_HPP
#define STK_SCOPEEXIT_HPP
#pragma once

#include <boost/preprocessor/cat.hpp>

namespace stk
{
    namespace detail
    {
        template <typename F>
        struct scope_exit
        {
            scope_exit(F f)
                : f(f)
            {}

            ~scope_exit()
            {
                f();
            }
            F f;
        };

        template <typename F>
        inline scope_exit<F> make_scope_exit(F f)
        {
            return scope_exit<F>(f);
        };
    }//! namespace detail;
}//! namespace simulation;

#define STK_SCOPE_EXIT(...)                                                                          \
    auto BOOST_PP_CAT(stk_scope_exit_, __LINE__) = stk::detail::make_scope_exit([&](){__VA_ARGS__;}) \
/***/

#define STK_MEMBER_SCOPE_EXIT(...)                                                                               \
    auto BOOST_PP_CAT(stk_member_scope_exit_, __LINE__) = stk::detail::make_scope_exit([this,&](){__VA_ARGS__;}) \
/***/

#endif// STK_SCOPEEXIT_HPP

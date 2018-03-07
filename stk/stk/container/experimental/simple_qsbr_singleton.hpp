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
#include <stk/container/simple_qsbr.hpp>

namespace stk { namespace thread {
    extern simple_qsbr& get_simple_qsbr_instance();
}}//! namespace stk::thread;

//! Place one copy of this in some translation unit.
#define STK_INSTANTIATE_SIMPLE_QSBR_INSTANCE() \
namespace stk { namespace thread {             \
    simple_qsbr& get_simple_qsbr_instance()    \
    {                                          \
        static simple_qsbr instance;           \
        return instance;                       \
    }                                          \
}}                                             \
/***/


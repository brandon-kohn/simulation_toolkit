//
//! Copyright © 2019
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/utility/detail/type_switch_base.hpp>
#include <stk/container/concurrent_numeric_unordered_map.hpp>
#include <boost/preprocessor.hpp>

#ifndef STK_TYPE_SWITCH_MAX_CASES 
    #define STK_TYPE_SWITCH_MAX_CASES 10
#endif

#define BOOST_PP_FILENAME_1 <stk/utility/detail/type_switch_n.hpp>
#define BOOST_PP_ITERATION_LIMITS (1, STK_TYPE_SWITCH_MAX_CASES)
#include BOOST_PP_ITERATE()


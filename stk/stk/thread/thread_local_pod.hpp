//
//! Copyright © 2018
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/config.hpp>

//! NOTE: Support for vc120's __declspec(thread). 
#if defined(BOOST_MSVC) && BOOST_MSVC < 1900 
    #define STK_THREAD_LOCAL_POD __declspec(thread)
#else
    #define STK_THREAD_LOCAL_POD thread_local
#endif

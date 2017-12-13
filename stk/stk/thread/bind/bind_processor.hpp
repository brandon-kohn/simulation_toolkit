
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef STK_BIND_TO_PROCESSOR_HPP
#define STK_BIND_TO_PROCESSOR_HPP

#ifdef __APPLE__
    #include <stk/thread/bind/bind_processor_macos.hpp>
#elif defined(__linux__)
    #include <stk/thread/bind/bind_processor_linux.hpp>
#elif defined(BOOST_WINDOWS)
    #include <stk/thread/bind/bind_processor_windows.hpp>
#else
    #error bind_to_processor not defined.
#endif

#endif//! STK_BIND_TO_PROCESSOR_HPP

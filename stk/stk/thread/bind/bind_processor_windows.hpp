
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
#ifndef STK_THREAD_BIND_PROCESSOR_LINUX_HPP
#define STK_THREAD_BIND_PROCESSOR_LINUX_HPP

#include <windows.h>
#include <stdexcept>
#include <boost/config/abi_prefix.hpp>

namespace stk{ namespace thread {

    inline bool bind_to_processor(unsigned int n)
    {
        return ::SetThreadAffinityMask(::GetCurrentThread(), (DWORD_PTR)1 << n) != 0;
    }

}}//! namespace stk::thread;

#include <boost/config/abi_suffix.hpp>

#endif//! STK_THREAD_BIND_PROCESSOR_LINUX_HPP
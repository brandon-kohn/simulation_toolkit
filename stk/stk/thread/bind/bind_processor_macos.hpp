//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_THREAD_BIND_PROCESSOR_MACOS_HPP
#define STK_THREAD_BIND_PROCESSOR_MACOS_HPP

#include <stdio.h>
#include <mach/thread_policy.h>
#include <mach/thread_act.h>
#include <pthread.h>

namespace stk { namespace thread {

    inline bool bind_to_processor(unsigned int cpu) 
    {
        thread_affinity_policy_data_t policy_data = { static_cast<int>(cpu) };
        thread_port_t threadport = pthread_mach_thread_np(pthread_self());
        return thread_policy_set(threadport, THREAD_AFFINITY_POLICY, (thread_policy_t)&policy_data, THREAD_AFFINITY_POLICY_COUNT) == KERN_SUCCESS;
    }

}}//! namespace stk::thread;

#endif//! STK_THREAD_BIND_PROCESSOR_MACOS_HPP

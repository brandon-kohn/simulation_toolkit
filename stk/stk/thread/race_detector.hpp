//
//! Copyright © 2019
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once
 
#include <geometrix/utility/assert.hpp>
#include <atomic>

namespace stk { namespace thread {

    struct race_detector
    {
        race_detector()
            : inUse{false}
        {}

        std::atomic<bool> inUse;
    };

    struct race_guard 
    {
        race_guard(race_detector& d)
            : detector(d)
        {
            auto isUsed = detector.inUse.exchange(true, std::memory_order_acquire);
            GEOMETRIX_ASSERT(!isUsed);
        }

        ~race_guard()
        {
            detector.inUse.store(false, std::memory_order_release);
        }

        race_detector& detector;
    };

}}//! namespace stk::thread;



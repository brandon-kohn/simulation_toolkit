//
//! Copyright © 2013-2018
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <cstdint>
#include <algorithm>

namespace stk { namespace thread {

    struct exp_backoff_policy
    {
        std::uint32_t min_delay = 1;
        std::uint32_t max_delay = 1000;
        std::uint32_t delay;

        exp_backoff_policy(std::uint32_t minDelay = 1, std::uint32_t maxDelay = 1000)
            : min_delay(minDelay)
            , max_delay( maxDelay )
            , delay(min_delay)
        {}

        void reset()
        {
            delay = min_delay;
        }

        template <typename Yield>
        void operator()(Yield fn)
        {
            auto d = delay;
            while(--d)
                fn();
            delay = (std::min)(max_delay, 2*delay);
        }
    };

    struct spin_backoff_policy
    {
        std::uint32_t spin_thresh = 100;
        std::uint32_t backoff_mult = 10;
        std::uint32_t spincount = 0;

        spin_backoff_policy(std::uint32_t spinThreshold = 100, std::uint32_t backoffMultiplier = 10)
            : spin_thresh(spinThreshold)
            , backoff_mult(backoffMultiplier)
        {}

        void reset()
        {
            spincount = 0;
        }

        template <typename Yield>
        void operator()(Yield fn)
        {
            if( ++spincount > spin_thresh )
            {
                auto backoff = spincount * backoff_mult;
                while(--backoff)
                    fn();
            }
        }
    };

    template <typename ThreadTraits, typename BackoffPolicy>
    struct backoff_policy : BackoffPolicy
    {
        template <typename... Args>
        backoff_policy(Args&&...a)
        : BackoffPolicy(std::forward<Args>(a)...)
        {}

        void operator()() { BackoffPolicy::operator()(ThreadTraits::yield);}

    };

}}//! namespace stk::thread;


//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/config.hpp>
#include <geometrix/utility/assert.hpp>
#include <stk/thread/cache_line_padding.hpp>
#include <stk/compiler/warnings.hpp>
#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

namespace stk { namespace thread {

    class scalable_task_counter
    {
STK_WARNING_PUSH()
#define STK_DISABLE STK_WARNING_PADDED
#include STK_DO_DISABLE_WARNING()
        struct BOOST_ALIGNMENT(STK_CACHE_LINE_SIZE) padded_atomic_counter : std::atomic<std::int64_t>
        {
            padded_atomic_counter()
                : std::atomic<std::int64_t>(0)
            {}
        };
STK_WARNING_POP()

    public:

        scalable_task_counter(std::uint32_t nthreads = std::thread::hardware_concurrency())
            : m_counts(nthreads)
        {}

        //! 0 is the main thread. [1..nthreads] are the pool threads.
        void increment(std::uint32_t tidx)
        {
            GEOMETRIX_ASSERT(tidx < m_counts.size());
            m_counts[tidx].fetch_add(1, std::memory_order_relaxed);
        }

        void decrement(std::uint64_t tidx)
        {
            GEOMETRIX_ASSERT(tidx < m_counts.size());
            m_counts[tidx].fetch_sub(1, std::memory_order_relaxed);
        }

        //! Counters are relaxed, so it's possible that this can be negative at times.
        std::int64_t count() const
        {
            std::int64_t sum = 0;
            for (auto& pc : m_counts)
                sum += pc.load(std::memory_order_relaxed);
            return sum;
        }

        void reset()
        {
            for (auto& pc : m_counts)
                pc.store(0, std::memory_order_relaxed);
        }

    private:
        BOOST_ALIGNMENT(STK_CACHE_LINE_SIZE) std::vector<padded_atomic_counter> m_counts;
    };

}}//! namespace stk::thread;

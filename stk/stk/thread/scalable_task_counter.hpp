//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <geometrix/utility/assert.hpp>
#include <stk/thread/cache_line_padding.hpp>
#include <atomic>
#include <cstdint>
#include <thread>

namespace stk { namespace thread {

    class scalable_task_counter
    {
        struct alignas(STK_CACHE_LINE_SIZE) padded_atomic_counter : std::atomic<std::int64_t>
        {
            padded_atomic_counter()
                : std::atomic<std::int64_t>{0}
            {}
        };

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
            GEOMETRIX_ASSERT(count() > 0);
            m_counts[tidx].fetch_sub(1, std::memory_order_relaxed);
        }

        std::int64_t count() const
        {
            std::int64_t sum = 0;
            for (auto& pc : m_counts)
                sum += pc.load(std::memory_order_relaxed);
            GEOMETRIX_ASSERT(sum >= 0);
            return sum;
        }

        void reset()
        {
            for (auto& pc : m_counts)
                pc.store(0, std::memory_order_relaxed);
        }

    private:
        alignas(STK_CACHE_LINE_SIZE) std::vector<padded_atomic_counter> m_counts;
    };

}}//! namespace stk::thread;

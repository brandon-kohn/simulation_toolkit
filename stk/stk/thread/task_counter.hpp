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

    class task_counter
    {
    public:

        task_counter(std::uint64_t /*nthreads*/ = 0)
            : m_counter(0)
        {}

        //! 0 is the main thread. [1..nthreads] are the pool threads.
        void increment(std::uint64_t)
        {
            m_counter.fetch_add(1, std::memory_order_relaxed);
        }

		void decrement(std::uint64_t)
        {
			GEOMETRIX_ASSERT(m_counter != 0);//Should have previously been incremented.
            m_counter.fetch_sub(1, std::memory_order_relaxed);
        }

        std::size_t count() const
        {
            return m_counter.load(std::memory_order_relaxed);
        }

        void reset()
        {
            m_counter.store(0, std::memory_order_relaxed);
        }

    private:
		alignas(STK_CACHE_LINE_SIZE)std::atomic<std::uint64_t> m_counter;
    };

}}//! namespace stk::thread;

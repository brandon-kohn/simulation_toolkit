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
#include <atomic>
#include <cstdint>
#include <thread>

namespace stk { namespace thread {

	class scalable_task_counter
	{
		using unique_atomic_uint32 = std::unique_ptr<std::atomic<std::uint32_t>>;
	public:

		scalable_task_counter(std::uint32_t nthreads = std::thread::hardware_concurrency())
		{
			while (nthreads--)
				m_counts.emplace_back(new std::atomic<std::uint32_t>(0));
		}

		//! 0 is the main thread. [1..nthreads] are the pool threads.
		void increment(std::uint32_t tidx)
		{
			GEOMETRIX_ASSERT(tidx < m_counts.size());
			m_counts[tidx]->fetch_add(1, std::memory_order_relaxed);
		}

		std::size_t count() const
		{
			std::size_t sum = 0;
			for (auto& pc : m_counts)
				sum += pc->load(std::memory_order_relaxed);
			return sum;
		}

		void reset() 
		{
			for (auto& pc : m_counts)
				pc->store(0, std::memory_order_relaxed);
		}

	private:
		std::vector<unique_atomic_uint32> m_counts;
	};

}}//! namespace stk::thread;

//
//! Copyright � 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <geometrix/numeric/number_comparison_policy.hpp>
#include <stk/thread/work_stealing_thread_pool.hpp>
#include <stk/thread/partition_work.hpp>
#include <boost/thread/futures/wait_for_all.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <geometrix/utility/scope_timer.ipp>
#include <geometrix/utility/ignore_unused_warnings.hpp>
#include <stk/thread/concurrentqueue.h>
#include <stk/thread/concurrentqueue_queue_info_no_tokens.h>
#include <stk/thread/boost_thread_kernel.hpp>
#include <stk/thread/scalable_task_counter.hpp>
#include <stk/utility/synthetic_work.hpp>
#include <stk/utility/time_execution.hpp>
#include <stk/thread/optimize_partition.hpp>

using mc_queue_traits = moodycamel_concurrent_queue_traits_no_tokens;

auto nOSThreads = static_cast<std::uint32_t>(std::max<std::size_t>(std::thread::hardware_concurrency()-1, 2));
using counter = stk::thread::scalable_task_counter;

struct work_stealing_thread_pool_fixture : ::testing::Test
{
    work_stealing_thread_pool_fixture()
        : pool(nOSThreads)
    {
    }

	//! Because fixtures are heap allocated and the pool is aligned due to padded members, the fixture must be allocated with cache line size alignment.
	void* operator new(std::size_t i)
	{
		return stk::aligned_alloc(i, STK_CACHE_LINE_SIZE);
	}

	void operator delete(void* p)
	{
		stk::free(p);
	}

	static const std::size_t nTimingRuns = 200;
    stk::thread::work_stealing_thread_pool<mc_queue_traits> pool;
};

TEST_F(work_stealing_thread_pool_fixture, optimize_partition)
{
	auto step = [](std::size_t n) { return n * 2; };
	auto job = [](std::size_t) { stk::synthetic_work(std::chrono::microseconds(50)); };
	auto result = stk::optimize_partition(pool, 10000, job, pool.number_threads()+1, step);

}


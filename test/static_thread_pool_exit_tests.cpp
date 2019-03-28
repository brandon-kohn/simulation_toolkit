//
//! Copyright © 2017
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
#include <stk/thread/thread_pool.hpp>
#include <stk/thread/partition_work.hpp>
#include <boost/thread/futures/wait_for_all.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <geometrix/utility/scope_timer.ipp>
#include <stk/thread/concurrentqueue.h>
#include <stk/thread/concurrentqueue_queue_info_no_tokens.h>
#include <stk/thread/boost_thread_kernel.hpp>
#include <stk/thread/task_counter.hpp>
#include <stk/thread/scalable_task_counter.hpp>

#include <boost/range/irange.hpp>
#include "thread_test_utils.hpp"

using mc_queue_traits = moodycamel_concurrent_queue_traits_no_tokens;

auto nOSThreads = std::max<std::size_t>(std::thread::hardware_concurrency()-1, 2);
using counter = stk::thread::scalable_task_counter;

struct singleton_work_stealing_thread_pool_fixture : ::testing::Test
{
    singleton_work_stealing_thread_pool_fixture()
    {}

	using pool_t = stk::thread::work_stealing_thread_pool<mc_queue_traits>;

	static pool_t& instance()
	{
		static stk::thread::work_stealing_thread_pool<mc_queue_traits> inst(nOSThreads);
		return inst;
	}
};

TEST_F(singleton_work_stealing_thread_pool_fixture, testStaticPool)
{
	auto& pool = instance();
	counter consumed(nOSThreads + 1);

	auto task = [&consumed](std::uint32_t) BOOST_NOEXCEPT{ consumed.increment(pool_t::get_thread_id()); synthetic_work(std::chrono::microseconds(10)); };
	for (auto i = 0ULL; i < 10000; ++i) 
	{
		consumed.reset();
		pool.parallel_for(boost::irange(0,10), task);
		EXPECT_EQ(10, consumed.count());
	}
}


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
#include <geometrix/test/gmessage.hpp>
#include <stk/thread/concurrentqueue.h>
#include <stk/thread/boost_thread_kernel.hpp>
#include <stk/thread/task_counter.hpp>
#include <stk/thread/scalable_task_counter.hpp>

#include <boost/range/irange.hpp>
#include "thread_test_utils.hpp"

STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(std::uint32_t);
auto keepReference = boost::context::stack_traits::default_size();
namespace stk {
    template <typename Fn, typename ... Ts>
    inline std::chrono::duration<double> time_execution(Fn&& fn, Ts&&... args)
    {
        auto start = std::chrono::high_resolution_clock::now();
        fn(std::forward<Ts>(args)...);
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>{ end - start };
    }
}//! namespace stk;

struct work_stealing_thread_pool_fixture : ::testing::TestWithParam<int>
{
    work_stealing_thread_pool_fixture()
        : pool(GetParam())
    {
    }

	static const std::size_t nTimingRuns = 200;
    stk::thread::work_stealing_thread_pool<moodycamel_concurrent_queue_traits> pool;
};

auto nOSThreads = std::thread::hardware_concurrency()-1;
template <std::size_t Runs>
struct timing_fixture : ::testing::Test
{
    timing_fixture()
    {
    }

	static const std::size_t nTimingRuns = Runs;
};

using timing_fixture200 = timing_fixture<200>;
TEST_F(timing_fixture200, test_partition_work)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;

    const int njobs = 64*1024;
    auto npartitions = nOSThreads;
    auto schedule = partition_work(njobs, npartitions);
    int count = 0;
    for (auto range : schedule)
        for (auto i = range.first; i < range.second; ++i)
            ++count;
    EXPECT_EQ(njobs, count);
}

TEST_F(timing_fixture200, test_partition_work_zero)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;

    const int njobs = 0;
    auto npartitions = nOSThreads;
    auto schedule = partition_work(njobs, npartitions);
    int count = 0;
    for (auto range : schedule)
        for (auto i = range.first; i < range.second; ++i)
            ++count;
    EXPECT_EQ(njobs, count);
}

TEST_F(timing_fixture200, test_partition_work_empty)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;

    const int njobs = 0;
    std::vector<int> items;
    auto npartitions = nOSThreads;
    auto schedule = partition_work(items, npartitions);
    int count = 0;
    for (auto range : schedule)
        for (auto i : range)
            ++count;
    EXPECT_EQ(njobs, count);
}

TEST_F(timing_fixture200, test_partition_work_one_item)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;

    const int njobs = 1;
    std::vector<int> items = { 1 };
    auto npartitions = nOSThreads;
    auto schedule = partition_work(items, npartitions);
    int count = 0;
    for (auto range : schedule)
        for (auto i : range)
            ++count;
    EXPECT_EQ(njobs, count);
}

TEST_F(timing_fixture200, test_partition_work_fewer_items_than_partitions)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;

    const int njobs = nOSThreads - 1;
    std::vector<int> items(njobs, 1);
    auto npartitions = nOSThreads;
    auto schedule = partition_work(items, npartitions);
    EXPECT_EQ(njobs, schedule.size());
    int count = 0;
    for (auto range : schedule)
        for (auto i : range)
            ++count;
    EXPECT_EQ(njobs, count);
}

const int njobs = 64 * 1024;

TEST_F(timing_fixture200, threads_moodycamel_concurrentQ_64k_empty_jobs_with_parallel_apply)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;

    thread_pool<moodycamel_concurrent_queue_traits> pool(nOSThreads);
    std::atomic<int> consumed{0};
    auto task = [&consumed](int) noexcept {consumed.fetch_add(1, std::memory_order_relaxed); };

    for (int i = 0; i < nTimingRuns; ++i)
    {
        consumed.store(0, std::memory_order_relaxed);
        {
            GEOMETRIX_MEASURE_SCOPE_TIME("thread_pool moody_64k empty with parallel_apply");
            pool.parallel_apply(njobs, task);
        }
        EXPECT_EQ(njobs, consumed.load(std::memory_order_relaxed));
    }
}

TEST_F(timing_fixture200, threads_moodycamel_concurrentQ_64k_empty_jobs_with_parallel_for)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;

    thread_pool<moodycamel_concurrent_queue_traits> pool(nOSThreads);

    std::atomic<int> consumed{0};
    auto task = [&consumed](int) noexcept {consumed.fetch_add(1, std::memory_order_relaxed); };

    for (int i = 0; i < nTimingRuns; ++i)
    {
        consumed.store(0, std::memory_order_relaxed);
        {
            GEOMETRIX_MEASURE_SCOPE_TIME("thread_pool moody_64k empty with parallel_for");
            pool.parallel_for(boost::irange(0,njobs), task);
        }
        EXPECT_EQ(njobs, consumed.load(std::memory_order_relaxed));
    }
}

TEST_P(work_stealing_thread_pool_fixture, work_stealing_threads_moodycamel_concurrentQ_64k_empty_jobs_enumerated)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;
    using pool_t = work_stealing_thread_pool<moodycamel_concurrent_queue_traits>;
    //std::vector<pool_t::template future<void>> fs;
    //std::atomic<int> consumed{0};
	task_counter consumed(GetParam()+1);
    auto qjobs = njobs;
    auto task = [&consumed]() noexcept {consumed.increment(pool_t::get_thread_id());};
    static_assert(noexcept(task()), "works.");
    std::stringstream name;
    name << GetParam() << " work-stealing threadpool moody_64k empty";

    for (int i = 0; i < nTimingRuns; ++i)
    {
        consumed.reset();
        {
            GEOMETRIX_MEASURE_SCOPE_TIME(name.str().c_str());
            for (auto q = 0; q < qjobs; ++q)
            {
                pool.send_no_future(pool.get_spinning_index(), task);
            }
            pool.wait_for([&consumed, qjobs]() noexcept { return consumed.count() == qjobs; });
        }
        EXPECT_EQ(qjobs, consumed.count());
    }
}
INSTANTIATE_TEST_CASE_P(work_stealing_threads_moodycamel_concurrentQ_64k_empty_jobs_enumerated, work_stealing_thread_pool_fixture, ::testing::Range(35, 36));

TEST_F(timing_fixture200, work_stealing_threads_moodycamel_concurrentQ_64k_empty_jobs_with_parallel_apply)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;
	using pool_t = work_stealing_thread_pool<moodycamel_concurrent_queue_traits>;
	pool_t pool(nOSThreads);
	scalable_task_counter consumed(nOSThreads + 1);

    auto task = [&consumed](int) noexcept {consumed.increment(pool_t::get_thread_id()); };
    for (int i = 0; i < nTimingRuns; ++i)
    {
        consumed.reset();
        {
            GEOMETRIX_MEASURE_SCOPE_TIME("work-stealing threadpool moody_64k empty with parallel_apply");
            pool.parallel_apply(njobs, task);
        }
        EXPECT_EQ(njobs, consumed.count());
    }
}

TEST_F(timing_fixture200, work_stealing_threads_moodycamel_concurrentQ_64k_empty_jobs_with_parallel_for)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;

	using pool_t = work_stealing_thread_pool<moodycamel_concurrent_queue_traits>;
	pool_t pool(nOSThreads);
	scalable_task_counter consumed(nOSThreads + 1);

	auto task = [&consumed](int) noexcept {consumed.increment(pool_t::get_thread_id()); };
	for (int i = 0; i < nTimingRuns; ++i)
	{
		consumed.reset();
		{
			GEOMETRIX_MEASURE_SCOPE_TIME("work-stealing threadpool moody_64k empty with parallel_for");
			pool.parallel_for(boost::irange(0, njobs), task);
		}
		EXPECT_EQ(njobs, consumed.count());
	}
}

using timing_fixture1 = timing_fixture<1>;
TEST_F(timing_fixture1, work_stealing_threads_moodycamel_concurrentQ_64k_100us_jobs_with_parallel_apply)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;
	using pool_t = work_stealing_thread_pool<moodycamel_concurrent_queue_traits>;
	pool_t pool(nOSThreads);
	scalable_task_counter consumed(nOSThreads + 1);

	auto task = [&consumed](int) noexcept {consumed.increment(pool_t::get_thread_id()); synthetic_work(std::chrono::microseconds(100)); };
    for (int i = 0; i < nTimingRuns; ++i)
    {
        consumed.reset();
        {
            GEOMETRIX_MEASURE_SCOPE_TIME("work-stealing threadpool moody_64k 100us with parallel_apply");
            pool.parallel_apply(njobs, task);
        }
        EXPECT_EQ(njobs, consumed.count());
    }
}

TEST_F(timing_fixture1, work_stealing_threads_moodycamel_concurrentQ_64k_100us_jobs_with_parallel_for)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;

	using pool_t = work_stealing_thread_pool<moodycamel_concurrent_queue_traits>;
	pool_t pool(nOSThreads);
	scalable_task_counter consumed(nOSThreads + 1);

	auto task = [&consumed](int) noexcept {consumed.increment(pool_t::get_thread_id()); synthetic_work(std::chrono::microseconds(100)); };
	for (int i = 0; i < nTimingRuns; ++i)
	{
		consumed.reset();
		{
			GEOMETRIX_MEASURE_SCOPE_TIME("work-stealing threadpool moody_64k 100us with parallel_for");
			pool.parallel_for(boost::irange(0, njobs), task);
		}
		EXPECT_EQ(njobs, consumed.count());
	}
}

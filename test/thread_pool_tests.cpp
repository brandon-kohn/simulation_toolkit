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
#include <geometrix/utility/ignore_unused_warnings.hpp>
#include <stk/thread/concurrentqueue.h>
#include <stk/thread/concurrentqueue_queue_info_no_tokens.h>
#include <stk/thread/boost_thread_kernel.hpp>
#include <stk/thread/task_counter.hpp>
#include <stk/thread/scalable_task_counter.hpp>
#include <stk/thread/vyukov_mpmc_queue.hpp>

#include <boost/range/irange.hpp>
#include "thread_test_utils.hpp"

using mc_queue_traits = moodycamel_concurrent_queue_traits_no_tokens;

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

auto nOSThreads = static_cast<std::uint32_t>(std::max<std::size_t>(std::thread::hardware_concurrency()-1, 2));
using counter = stk::thread::scalable_task_counter;

struct work_stealing_thread_pool_fixture : ::testing::TestWithParam<int>
{
    work_stealing_thread_pool_fixture()
        : pool(GetParam())
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

    const std::size_t njobs = 64*1024;
    auto npartitions = nOSThreads;
    auto schedule = partition_work(njobs, npartitions);
    std::size_t count = 0;
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

    const std::size_t njobs = 0;
    auto npartitions = nOSThreads;
    auto schedule = partition_work(njobs, npartitions);
    std::size_t count = 0;
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

    const std::size_t njobs = 0;
    std::vector<int> items;
    auto npartitions = nOSThreads;
    auto schedule = partition_work(items, npartitions);
    std::size_t count = 0;
    for (auto range : schedule)
		for (auto i : range)
		{
			geometrix::ignore_unused_warning_of(i);
			++count;
		}
    EXPECT_EQ(njobs, count);
}

TEST_F(timing_fixture200, test_partition_work_one_item)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;

    const std::size_t njobs = 1;
    std::vector<int> items = { 1 };
    auto npartitions = nOSThreads;
    auto schedule = partition_work(items, npartitions);
    std::size_t count = 0;
    for (auto range : schedule)
		for (auto i : range)
		{
			geometrix::ignore_unused_warning_of(i);
			++count;
		}
    EXPECT_EQ(njobs, count);
}

TEST_F(timing_fixture200, test_partition_work_fewer_items_than_partitions)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;

	GTEST_MESSAGE("nOSThreads: ") << nOSThreads;
    const std::size_t njobs = nOSThreads - 1;
    std::vector<int> items(njobs, 1);
    auto npartitions = nOSThreads;
    auto schedule = partition_work(items, npartitions);
    EXPECT_EQ(njobs, schedule.size());
    std::size_t count = 0;
    for (auto range : schedule)
		for (auto i : range)
		{
			geometrix::ignore_unused_warning_of(i);
			++count;
		}
    EXPECT_EQ(njobs, count);
}

#define STK_DO_THREADPOOL_TIMINGS
#ifdef STK_DO_THREADPOOL_TIMINGS
const std::size_t njobs = 64 * 1024;
TEST_F(timing_fixture200, threads_moodycamel_concurrentQ_64k_empty_jobs_with_parallel_apply)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;

	GTEST_MESSAGE("Starting pool with nthreads: ") << nOSThreads;
    thread_pool<mc_queue_traits> pool(nOSThreads);
	GTEST_MESSAGE("Running timings: ") << nOSThreads;
    std::atomic<std::int64_t> consumed{0};
    auto task = [&consumed](std::ptrdiff_t) BOOST_NOEXCEPT {consumed.fetch_add(1, std::memory_order_relaxed); };

	try
	{
		for (auto i = 0ULL; i < nTimingRuns; ++i)
		{
			consumed.store(0, std::memory_order_relaxed);
			{
				GEOMETRIX_MEASURE_SCOPE_TIME("thread_pool moody_64k empty with parallel_apply");
				pool.parallel_apply(njobs, task);
			}
			EXPECT_EQ(njobs, consumed.load(std::memory_order_relaxed));
		}
	} catch (std::exception& e)
	{
		GTEST_MESSAGE("exception: ") << e.what();
		GTEST_MESSAGE("consumed count: ") << consumed;
		throw e;
	}
	GTEST_MESSAGE("consumed count: ") << consumed;
}

TEST_F(timing_fixture200, threads_moodycamel_concurrentQ_64k_empty_jobs_with_parallel_for)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;

    thread_pool<mc_queue_traits> pool(nOSThreads);

    std::atomic<std::int64_t> consumed{0};
    auto task = [&consumed](std::ptrdiff_t) BOOST_NOEXCEPT {consumed.fetch_add(1, std::memory_order_relaxed); };

    for (auto i = 0ULL; i < nTimingRuns; ++i)
    {
        consumed.store(0, std::memory_order_relaxed);
        {
            GEOMETRIX_MEASURE_SCOPE_TIME("thread_pool moody_64k empty with parallel_for");
            pool.parallel_for(boost::irange<std::size_t>(0,njobs), task);
        }
        EXPECT_EQ(njobs, consumed.load(std::memory_order_relaxed));
    }
}

TEST_P(work_stealing_thread_pool_fixture, work_stealing_threads_moodycamel_concurrentQ_64k_empty_jobs_enumerated)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;
    using pool_t = work_stealing_thread_pool<mc_queue_traits>;
	counter consumed(GetParam()+1);
    auto qjobs = njobs;
    auto task = [&consumed]() BOOST_NOEXCEPT {consumed.increment(pool_t::get_thread_id());};
    std::stringstream name;
    name << GetParam() << " work-stealing threadpool moody_64k empty";

    for (auto i = 0ULL; i < nTimingRuns; ++i)
    {
        consumed.reset();
        {
            GEOMETRIX_MEASURE_SCOPE_TIME(name.str().c_str());
            for (auto q = 0; q < qjobs; ++q)
            {
				std::uint32_t threadID = q % (GetParam() - 1) + 1;
                pool.send_no_future(threadID, task);
            }
			pool.wait_for_all_tasks();
            //pool.wait_for([&consumed, qjobs]() BOOST_NOEXCEPT { return consumed.count() == qjobs; });
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
	using pool_t = work_stealing_thread_pool<mc_queue_traits>;
	
	GTEST_MESSAGE("Starting pool with nthreads: ") << nOSThreads;
	pool_t pool(nOSThreads);
	counter consumed(nOSThreads + 1);

	GTEST_MESSAGE("Running timings: ") << nOSThreads;
	try
	{
		auto task = [&consumed](std::ptrdiff_t) BOOST_NOEXCEPT {consumed.increment(pool_t::get_thread_id()); };
	    for (auto i = 0ULL; i < nTimingRuns; ++i)
		{
			consumed.reset();
			{
				GEOMETRIX_MEASURE_SCOPE_TIME("work-stealing threadpool moody_64k empty with parallel_apply");
				pool.parallel_apply(njobs, task);
			}
			EXPECT_EQ(njobs, consumed.count());
		}
	} catch (std::exception& e)
	{
		GTEST_MESSAGE("exception: ") << e.what();
		GTEST_MESSAGE("consumed count: ") << consumed.count();
		throw e;
	}
	GTEST_MESSAGE("consumed count: ") << consumed.count();
}

TEST_F(timing_fixture200, work_stealing_threads_moodycamel_concurrentQ_64k_empty_jobs_with_parallel_for)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;

	using pool_t = work_stealing_thread_pool<mc_queue_traits>;
	pool_t pool(nOSThreads);
	counter consumed(nOSThreads + 1);

	auto task = [&consumed](std::ptrdiff_t) BOOST_NOEXCEPT {consumed.increment(pool_t::get_thread_id()); };
	for (auto i = 0ULL; i < nTimingRuns; ++i)
	{
		consumed.reset();
		{
			GEOMETRIX_MEASURE_SCOPE_TIME("work-stealing threadpool moody_64k empty with parallel_for");
			pool.parallel_for(boost::irange<std::size_t>(0, njobs), task);
		}
		EXPECT_EQ(njobs, consumed.count());
	}
}

TEST_F(timing_fixture200, work_stealing_threads_vyukov_concurrentQ_64k_empty_jobs_with_parallel_for)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;

	using pool_t = work_stealing_thread_pool<vyukov_mpmc_queue_traits>;
	pool_t pool(nOSThreads);
	counter consumed(nOSThreads + 1);

	auto task = [&consumed](std::ptrdiff_t) BOOST_NOEXCEPT {consumed.increment(pool_t::get_thread_id()); };
	for (auto i = 0ULL; i < nTimingRuns; ++i)
	{
		consumed.reset();
		{
			GEOMETRIX_MEASURE_SCOPE_TIME("work-stealing threadpool vyukov_64k empty with parallel_for");
			pool.parallel_for(boost::irange<std::size_t>(0, njobs), task);
		}
		EXPECT_EQ(njobs, consumed.count());
	}
}


using timing_fixture1 = timing_fixture<1>;
TEST_F(timing_fixture1, work_stealing_threads_moodycamel_concurrentQ_64k_1000us_jobs_with_parallel_apply)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;
	using pool_t = work_stealing_thread_pool<mc_queue_traits>;
	pool_t pool(nOSThreads);
	counter consumed(nOSThreads + 1);

	auto task = [&consumed](std::ptrdiff_t) BOOST_NOEXCEPT {consumed.increment(pool_t::get_thread_id()); synthetic_work(std::chrono::microseconds(1000)); };
    for (auto i = 0ULL; i < nTimingRuns; ++i)
    {
        consumed.reset();
        {
            GEOMETRIX_MEASURE_SCOPE_TIME("work-stealing threadpool moody_64k 1000us with parallel_apply");
            pool.parallel_apply(njobs, task);
        }
        EXPECT_EQ(njobs, consumed.count());
    }
}

TEST_F(timing_fixture1, work_stealing_threads_moodycamel_concurrentQ_64k_1000us_jobs_with_parallel_for)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;

	using pool_t = work_stealing_thread_pool<mc_queue_traits>;
	pool_t pool(nOSThreads);
	counter consumed(nOSThreads + 1);

	auto task = [&consumed](std::ptrdiff_t) BOOST_NOEXCEPT {consumed.increment(pool_t::get_thread_id()); synthetic_work(std::chrono::microseconds(1000)); };
	for (auto i = 0ULL; i < nTimingRuns; ++i)
	{
		consumed.reset();
		{
			GEOMETRIX_MEASURE_SCOPE_TIME("work-stealing threadpool moody_64k 1000us with parallel_for");
			pool.parallel_for(boost::irange<std::size_t>(0, njobs), task);
		}
		EXPECT_EQ(njobs, consumed.count());
	}
}

TEST_F(timing_fixture1, work_stealing_threads_moodycamel_concurrentQ_no_tokens_64k_1000us_jobs_with_parallel_for)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;

	using pool_t = work_stealing_thread_pool<moodycamel_concurrent_queue_traits_no_tokens>;
	pool_t pool(nOSThreads);
	counter consumed(nOSThreads + 1);

	auto task = [&consumed](std::ptrdiff_t) BOOST_NOEXCEPT {consumed.increment(pool_t::get_thread_id()); synthetic_work(std::chrono::microseconds(1000)); };
	for (auto i = 0ULL; i < nTimingRuns; ++i)
	{
		consumed.reset();
		{
			GEOMETRIX_MEASURE_SCOPE_TIME("work-stealing threadpool moody_no_tokens_64k 1000us with parallel_for");
			pool.parallel_for(boost::irange<std::size_t>(0, njobs), task);
		}
		EXPECT_EQ(njobs, consumed.count());
	}
}
#endif//DO TIMINGS

TEST(cache_line_padding_suite, test_simple_padding)
{
	EXPECT_EQ(STK_CACHE_LINE_SIZE, sizeof(stk::thread::padded<int>));
	EXPECT_EQ(STK_CACHE_LINE_SIZE, sizeof(stk::thread::padded<double>));
	EXPECT_EQ(STK_CACHE_LINE_SIZE, sizeof(stk::thread::padded<bool>));
	EXPECT_EQ(STK_CACHE_LINE_SIZE, sizeof(stk::thread::padded<std::atomic<bool>>));
}

TEST(work_stealing_thread_pool_test_suite, test_aligned_alloc)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	
	using pool_t = work_stealing_thread_pool<moodycamel_concurrent_queue_traits_no_tokens>;
	auto pool = boost::make_unique<pool_t>(nOSThreads);
}

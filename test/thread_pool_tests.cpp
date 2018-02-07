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
#include <stk/thread/partition_work.hpp>
#include <boost/thread/futures/wait_for_all.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <geometrix/utility/scope_timer.ipp>
#include <stk/thread/concurrentqueue.h>
#include <chrono>

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

auto nOSThreads = std::thread::hardware_concurrency()-1;

TEST(timing, test_partition_work)
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

size_t nTimingRuns = 20;

#include <stk/thread/LockLessMultiReadPipe.h>
TEST(timing, work_stealing_threads_enki_concurrentQ_64k_empty_jobs)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;
    using future_t = boost::future<void>;

    work_stealing_thread_pool<enkiTS_concurrent_queue_traits> pool(nOSThreads);
    const int njobs = 64 * 1024;
    std::vector<future_t> fs;
    fs.reserve(njobs);
	std::atomic<int> consumed = 0;
    for (int i = 0; i < nTimingRuns; ++i)
    {
        {
            GEOMETRIX_MEASURE_SCOPE_TIME("enki_64k empty");
            for(int n = 0; n < njobs; ++n)
            {
				fs.emplace_back(pool.send([&consumed]() {consumed.fetch_add(1, std::memory_order_relaxed); }));
            }

            boost::for_each(fs, [](const future_t& f) { f.wait(); });
        }
        fs.clear();
    }

    EXPECT_EQ(njobs*nTimingRuns, consumed.load(std::memory_order_relaxed));
}

TEST(timing, work_stealing_threads_moodycamel_concurrentQ_64k_empty_jobs)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;
    using future_t = boost::future<void>;

    work_stealing_thread_pool<moodycamel_concurrent_queue_traits> pool(nOSThreads);
    const int njobs = 64 * 1024;
    std::vector<future_t> fs;
    fs.reserve(njobs);
	std::atomic<int> consumed = 0;
    for (int i = 0; i < nTimingRuns; ++i)
    {
        {
            GEOMETRIX_MEASURE_SCOPE_TIME("moody_64k empty");
            for(int n = 0; n < njobs; ++n)
            {
				fs.emplace_back(pool.send([&consumed]() {consumed.fetch_add(1, std::memory_order_relaxed); }));
            }

            boost::for_each(fs, [](const future_t& f) { f.wait(); });
        }
        fs.clear();
    }

    EXPECT_EQ(njobs*nTimingRuns, consumed.load(std::memory_order_relaxed));
}

TEST(timing, work_stealing_threads_moodycamel_concurrentQ_64k_empty_jobs_with_creator_task)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;
    using future_t = boost::future<void>;

    work_stealing_thread_pool<moodycamel_concurrent_queue_traits> pool(nOSThreads);
    const int njobs = 64 * 1024;

	std::atomic<int> consumed = 0;
	auto task = [&consumed]() {consumed.fetch_add(1, std::memory_order_relaxed); };
	auto add_tasks = [&pool, &task]() 
	{ 
		for(int i = 0; i < 1024; ++i) 
			pool.send_no_future(task);
	};
	for (int i = 0; i < nTimingRuns; ++i)
    {
		consumed.store(0, std::memory_order_relaxed);
        {
            GEOMETRIX_MEASURE_SCOPE_TIME("moody_64k empty with creator");
            for(int n = 0; n < 64; ++n)
				pool.send_no_future(add_tasks);
			pool.wait_for([&consumed, njobs]()
			{ 
				return consumed.load(std::memory_order_relaxed) == njobs;
			});
        }
		EXPECT_EQ(njobs, consumed.load(std::memory_order_relaxed));
    }
}

TEST(timing, work_stealing_threads_moodycamel_concurrentQ_64k_empty_jobs_with_creator_task_and_schedule)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;
    using future_t = boost::future<void>;

    work_stealing_thread_pool<moodycamel_concurrent_queue_traits> pool(nOSThreads);
    const int njobs = 64 * 1024;
	auto npartitions = nOSThreads;
	auto schedule = partition_work(njobs, npartitions);

	std::atomic<int> consumed = 0;
	auto task = [&consumed]() {consumed.fetch_add(1, std::memory_order_relaxed); };
	for (int i = 0; i < nTimingRuns; ++i)
    {
		consumed.store(0, std::memory_order_relaxed);
        {
            GEOMETRIX_MEASURE_SCOPE_TIME("moody_64k empty with creator and schedule");
			for (auto& range : schedule)
			{
				auto add_tasks = [&pool, &task, range]()
				{ 
					for (int i = range.first; i < range.second; ++i)
						pool.send_no_future(task);
				};
				pool.send_no_future(add_tasks);
			}
			pool.wait_for([&consumed, njobs](){ return consumed.load(std::memory_order_relaxed) == njobs; });
        }
		EXPECT_EQ(njobs, consumed.load(std::memory_order_relaxed));
    }
}

#include <boost/range/irange.hpp>
TEST(timing, work_stealing_threads_moodycamel_concurrentQ_64k_empty_jobs_with_parallel_for)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;
    using future_t = boost::future<void>;

    work_stealing_thread_pool<moodycamel_concurrent_queue_traits> pool(nOSThreads);
    const int njobs = 64 * 1024;

	std::atomic<int> consumed = 0;
	auto task = [&consumed](int) {consumed.fetch_add(1, std::memory_order_relaxed); };
	
	for (int i = 0; i < nTimingRuns; ++i)
    {
		consumed.store(0, std::memory_order_relaxed);
        {
            GEOMETRIX_MEASURE_SCOPE_TIME("moody_64k empty with creator and schedule");
			pool.parallel_for(boost::irange(0, njobs), task);
		}
		EXPECT_EQ(njobs, consumed.load(std::memory_order_relaxed));
    }
}

//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <stk/test/gtest_custom_message.hpp>

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
            for (auto q = decltype(qjobs){}; q < qjobs; ++q)
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

TEST(work_stealing_thread_pool_test_suite, exception_thrown)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	
	using pool_t = work_stealing_thread_pool<moodycamel_concurrent_queue_traits_no_tokens>;
	auto pool = boost::make_unique<pool_t>(nOSThreads);

	auto f = pool->send( []()
		{ throw std::logic_error{ "logic is wrong" }; } );

	EXPECT_THROW( f.get(), std::logic_error );
}

//
//! Work-stealing pool micro-benchmarks (gtest-driven)
//
//! Enable these tests only when you want timings:
//!   add /DSTK_DO_WORK_STEALING_POOL_BENCH to your test compile defines
//

#include <geometrix/utility/scope_timer.ipp> //! for GEOMETRIX_MEASURE_SCOPE_TIME (repo uses this in timing tests) :contentReference[oaicite:2]{index=2}

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include <intrin.h>
#include <chrono>
#include <cstdint>
#include <thread>
#include <atomic>

static inline std::uint64_t rdtsc() { return __rdtsc(); }

static inline std::uint64_t calibrate_cycles_per_us()
{
	using clock_t = std::chrono::steady_clock;

	constexpr auto sample = std::chrono::milliseconds( 250 );

	const auto          t0 = clock_t::now();
	const std::uint64_t c0 = rdtsc();

	while( clock_t::now() - t0 < sample )
		_mm_pause();

	const std::uint64_t c1 = rdtsc();
	const auto          us = std::chrono::duration_cast<std::chrono::microseconds>( sample ).count();

	const double cycles_per_us = static_cast<double>( c1 - c0 ) / static_cast<double>( us );
	return static_cast<std::uint64_t>( cycles_per_us );
}

static inline void spin_cycles( std::uint64_t cycles )
{
	const std::uint64_t start = rdtsc();
	while( ( rdtsc() - start ) < cycles )
		_mm_pause();

	std::atomic_signal_fence( std::memory_order_seq_cst );
}

struct SpinUs
{
	std::uint64_t cycles_per_us = 0;

	SpinUs()
		: cycles_per_us( calibrate_cycles_per_us() )
	{}

	void operator()( std::uint64_t us ) const
	{
		spin_cycles( cycles_per_us * us );
	}
};

//! A tiny “spin for ~N nanoseconds” payload.
//! This avoids Sleep jitter and keeps the work mostly on-core.
//! NOTE: for very small targets (like ~40us), the timing resolution and CPU freq scaling can matter.

static inline void spin_for_ns( std::int64_t ns )
{
	if( ns <= 0 )
		return;
	auto start = std::chrono::high_resolution_clock::now();
	while( true )
	{
		auto now = std::chrono::high_resolution_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>( now - start ).count();
		if( elapsed >= ns )
			break;
		//! prevent overly-smart optimization
		std::atomic_signal_fence( std::memory_order_seq_cst );
	}
}

using clock_type = std::chrono::high_resolution_clock;

static inline double seconds_since( clock_type::time_point t0 )
{
	return std::chrono::duration<double>( clock_type::now() - t0 ).count();
}

#define STK_DO_WORK_STEALING_POOL_BENCH
#ifdef STK_DO_WORK_STEALING_POOL_BENCH

TEST( timing, work_stealing_pool_send_future_overhead )
{
	using namespace stk::thread;

	//! match patterns already used in repo tests :contentReference[oaicite:3]{index=3}
	using pool_t = work_stealing_thread_pool<mc_queue_traits>;
	pool_t pool{ static_cast<std::uint32_t>( (std::max)( 1u, std::thread::hardware_concurrency() - 1 ) ) };

	using future_t = pool_t::template future<void>;

	constexpr std::size_t nTasks = 200000; //! tune this up/down
	std::vector<future_t> fs;
	fs.reserve( nTasks );

	{
		GEOMETRIX_MEASURE_SCOPE_TIME( "ws_pool/send(future) empty_task" );
		for( std::size_t i = 0; i < nTasks; ++i )
			fs.emplace_back( pool.send( []() -> void {} ) );
		for( auto& f : fs )
			f.wait();
	}

	//! sanity: force any exceptions to surface (pattern used elsewhere) :contentReference[oaicite:4]{index=4}
	for( auto& f : fs )
		EXPECT_NO_THROW( f.get() );
}

TEST( timing, work_stealing_pool_send_no_future_overhead )
{
	using namespace stk::thread;

	using pool_t = work_stealing_thread_pool<mc_queue_traits>;
	pool_t pool{ static_cast<std::uint32_t>( (std::max)( 1u, std::thread::hardware_concurrency() - 1 ) ) };

	constexpr std::size_t    nTasks = 200000; //! tune this up/down
	std::atomic<std::size_t> done{ 0 };

	{
		GEOMETRIX_MEASURE_SCOPE_TIME( "ws_pool/send_no_future empty_task" );
		for( std::size_t i = 0; i < nTasks; ++i )
		{
			//! API exists in stk synopsis (noexcept precondition) :contentReference[oaicite:5]{index=5}
			pool.send_no_future( [&done]() noexcept
				{ done.fetch_add( 1, std::memory_order_relaxed ); } );
		}
		while( done.load( std::memory_order_relaxed ) != nTasks )
			std::this_thread::yield();
	}

	EXPECT_EQ( done.load(), nTasks );
}

TEST( timing, work_stealing_pool_parallel_apply_payload_40us )
{
	using namespace stk::thread;

	using pool_t = work_stealing_thread_pool<mc_queue_traits>;
	pool_t pool{ static_cast<std::uint32_t>( (std::max)( 1u, std::thread::hardware_concurrency() - 1 ) ) };

	//! “How about 40 microseconds?”
	constexpr std::int64_t   payload_ns = 40'000; //! 40us
	constexpr std::ptrdiff_t nItems = 20000;      //! tune to get stable timings

	std::atomic<std::uint64_t> sink{ 0 };

	{
		//! parallel_apply is used throughout existing tests :contentReference[oaicite:6]{index=6}
		GEOMETRIX_MEASURE_SCOPE_TIME( "ws_pool/parallel_apply payload_40us" );
		pool.parallel_apply( nItems, [&]( std::ptrdiff_t i )
			{
            spin_for_ns(payload_ns);
            sink.fetch_add(static_cast<std::uint64_t>(i), std::memory_order_relaxed); } );
	}

	EXPECT_NE( sink.load( std::memory_order_relaxed ), 0ULL );
}

TEST( timing, work_stealing_pool_send_round_robin_payload_40us )
{
	using namespace stk::thread;

	using pool_t = work_stealing_thread_pool<mc_queue_traits>;
	pool_t pool{ static_cast<std::uint32_t>( (std::max)( 1u, std::thread::hardware_concurrency() - 1 ) ) };

	using future_t = pool_t::template future<void>;

	constexpr std::int64_t payload_ns = 40'000; //! 40us
	constexpr std::size_t  nTasks = 20000;     //! tune

	std::vector<future_t> fs;
	fs.reserve( nTasks );

	{
		//! pool.send(threadIndex, ...) is in the stk synopsis :contentReference[oaicite:7]{index=7}
		GEOMETRIX_MEASURE_SCOPE_TIME( "ws_pool/send(threadIndex) payload_40us" );
		for( std::size_t i = 0; i < nTasks; ++i )
		{
			auto tid = static_cast<std::uint32_t>( i % pool.number_threads() );
			fs.emplace_back( pool.send( tid, [=]() -> void
				{ spin_for_ns( payload_ns ); } ) );
		}
		for( auto& f : fs )
			f.wait();
	}
}
TEST( timing, sequential_baseline_payload_40us )
{
	constexpr std::int64_t   payload_ns = 40'000; //! 40us
	constexpr std::ptrdiff_t nItems = 20'000;     //! keep this identical to your parallel test

	std::atomic<std::uint64_t> sink{ 0 };

	{
		GEOMETRIX_MEASURE_SCOPE_TIME( "sequential/baseline payload_40us" );
		for( std::ptrdiff_t i = 0; i < nItems; ++i )
		{
			spin_for_ns( payload_ns );
			sink.fetch_add( static_cast<std::uint64_t>( i ), std::memory_order_relaxed );
		}
	}

	EXPECT_NE( sink.load( std::memory_order_relaxed ), 0ULL );
}

TEST( timing, sequential_baseline_payload_400us )
{
	constexpr std::int64_t   payload_ns = 400'000; //! 400us
	constexpr std::ptrdiff_t nItems = 5'000;       //! choose so runtime is similar to the 40us case

	std::atomic<std::uint64_t> sink{ 0 };

	{
		GEOMETRIX_MEASURE_SCOPE_TIME( "sequential/baseline payload_400us" );
		for( std::ptrdiff_t i = 0; i < nItems; ++i )
		{
			spin_for_ns( payload_ns );
			sink.fetch_add( static_cast<std::uint64_t>( i ), std::memory_order_relaxed );
		}
	}

	EXPECT_NE( sink.load( std::memory_order_relaxed ), 0ULL );
}

static double percentile_us( std::vector<double>& v, double p )
{
	if( v.empty() )
		return 0.0;
	std::sort( v.begin(), v.end() );
	const double      idx = ( p / 100.0 ) * ( v.size() - 1 );
	const std::size_t lo = static_cast<std::size_t>( idx );
	const std::size_t hi = (std::min)( lo + 1, v.size() - 1 );
	const double      frac = idx - static_cast<double>( lo );
	return v[lo] * ( 1.0 - frac ) + v[hi] * frac;
}

TEST( timing, ws_pool_burst_latency_40us_5_10_20 )
{
	using namespace stk::thread;

	using pool_t = work_stealing_thread_pool<mc_queue_traits>;
	pool_t pool{ static_cast<std::uint32_t>(
		(std::max)( 1u, std::thread::hardware_concurrency() - 1 ) ) };

	const std::uint64_t cycles_per_us = calibrate_cycles_per_us();
	const std::uint64_t cycles_40us = cycles_per_us * 40;

	using future_t = pool_t::template future<void>;

	const int burst_sizes[] = { 5, 10, 20 };

	constexpr int warmup_bursts = 200;
	constexpr int measure_bursts = 2000;

	for( int nTasks : burst_sizes )
	{
		std::vector<future_t> fs( static_cast<std::size_t>( nTasks ) );

		//! Warmup (no measurement)
		for( int b = 0; b < warmup_bursts; ++b )
		{
			for( int i = 0; i < nTasks; ++i )
				fs[static_cast<std::size_t>( i )] = pool.send( [=]() -> void
					{ spin_cycles( cycles_40us ); } );
			for( auto& f : fs )
				f.wait();
		}

		//! Measure per-burst makespan
		std::vector<double> burst_us;
		burst_us.reserve( measure_bursts );

		for( int b = 0; b < measure_bursts; ++b )
		{
			const auto t0 = clock_type::now();

			for( int i = 0; i < nTasks; ++i )
				fs[static_cast<std::size_t>( i )] = pool.send( [=]() -> void
					{ spin_cycles( cycles_40us ); } );

			for( auto& f : fs )
				f.wait();

			const auto   t1 = clock_type::now();
			const double us = std::chrono::duration<double, std::micro>( t1 - t0 ).count();
			burst_us.push_back( us );
		}

		const double p50 = percentile_us( burst_us, 50.0 );
		const double p90 = percentile_us( burst_us, 90.0 );
		const double p99 = percentile_us( burst_us, 99.0 );

		//! Sequential baseline for comparison (same burst sizes)
		const auto s0 = clock_type::now();
		for( int b = 0; b < measure_bursts; ++b )
			for( int i = 0; i < nTasks; ++i )
				spin_cycles( cycles_40us );
		const auto s1 = clock_type::now();

		const double seq_total_us = std::chrono::duration<double, std::micro>( s1 - s0 ).count();
		const double seq_per_burst_us = seq_total_us / static_cast<double>( measure_bursts );

		const double speedup_p50 = seq_per_burst_us / p50;

		std::cout
			<< "[BurstLatency40us] nTasks=" << nTasks
			<< " seq_per_burst_us=" << seq_per_burst_us
			<< " p50_us=" << p50
			<< " p90_us=" << p90
			<< " p99_us=" << p99
			<< " speedup_p50=" << speedup_p50
			<< "\n";

		RecordProperty( ( "burst_" + std::to_string( nTasks ) + "_seq_per_burst_us" ).c_str(), seq_per_burst_us );
		RecordProperty( ( "burst_" + std::to_string( nTasks ) + "_p50_us" ).c_str(), p50 );
		RecordProperty( ( "burst_" + std::to_string( nTasks ) + "_p90_us" ).c_str(), p90 );
		RecordProperty( ( "burst_" + std::to_string( nTasks ) + "_p99_us" ).c_str(), p99 );
		RecordProperty( ( "burst_" + std::to_string( nTasks ) + "_speedup_p50" ).c_str(), speedup_p50 );

		ASSERT_GT( p50, 0.0 );
	}
}

TEST( timing, ws_pool_small_burst_40us_SANITY_CHECK )
{
	using namespace stk::thread;

	using pool_t = work_stealing_thread_pool<mc_queue_traits>;
	pool_t pool{ static_cast<std::uint32_t>(
		(std::max)( 1u, std::thread::hardware_concurrency() - 1 ) ) };

	const std::uint64_t cycles_per_us = calibrate_cycles_per_us();
	const std::uint64_t cycles_40us = cycles_per_us * 40;

	constexpr int nTasks = 10;
	constexpr int repeats = 1000;

	using future_t = pool_t::template future<void>;
	std::vector<future_t> fs( nTasks );

	std::atomic<std::uint64_t> sum_cycles{ 0 };
	std::atomic<std::uint64_t> max_cycles{ 0 };
	std::atomic<int>           completed{ 0 };

	auto update_max = [&]( std::uint64_t v )
	{
		std::uint64_t cur = max_cycles.load( std::memory_order_relaxed );
		while( cur < v && !max_cycles.compare_exchange_weak( cur, v, std::memory_order_relaxed ) )
		{}
	};

	for( int r = 0; r < repeats; ++r )
	{
		for( int i = 0; i < nTasks; ++i )
		{
			fs[i] = pool.send( [&]() -> void
				{
                const auto c0 = rdtsc();
                spin_cycles(cycles_40us);
                const auto c1 = rdtsc();

                const auto d = (c1 - c0);
                sum_cycles.fetch_add(d, std::memory_order_relaxed);
                update_max(d);

                completed.fetch_add(1, std::memory_order_relaxed); } );
		}

		for( auto& f : fs )
			f.wait();
	}

	const int expected = nTasks * repeats;

	EXPECT_EQ( completed.load( std::memory_order_relaxed ), expected )
		<< "Not all tasks executed!";

	const double avg_cycles = static_cast<double>( sum_cycles.load( std::memory_order_relaxed ) ) / expected;

	const double avg_us = avg_cycles / static_cast<double>( cycles_per_us );
	const double max_us = static_cast<double>( max_cycles.load( std::memory_order_relaxed ) ) / static_cast<double>( cycles_per_us );

	std::cout
		<< "[SANITY] avg_task_us=" << avg_us
		<< " max_task_us=" << max_us
		<< " target_us=40\n";

	//! If this fails, your payload is not actually ~40us
	EXPECT_GT( avg_us, 30.0 );
	EXPECT_LT( avg_us, 60.0 );
}

#endif //! STK_DO_WORK_STEALING_POOL_BENCH

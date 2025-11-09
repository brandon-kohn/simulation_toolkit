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
#include <geometrix/utility/ignore_unused_warnings.hpp>
#include <stk/thread/concurrentqueue.h>
#include <stk/thread/concurrentqueue_queue_info_no_tokens.h>
#include <stk/thread/boost_thread_kernel.hpp>
#include <stk/thread/scalable_task_counter.hpp>
#include <stk/utility/synthetic_work.hpp>
#include <stk/utility/time_execution.hpp>
#include <stk/thread/optimize_partition.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <boost/unordered/concurrent_node_map.hpp>
#include <stk/thread/task_system.hpp>

using mc_queue_traits = moodycamel_concurrent_queue_traits_no_tokens;

auto nOSThreads = static_cast<std::uint32_t>(std::max<std::size_t>(std::thread::hardware_concurrency()-1, 2));
using counter = stk::thread::scalable_task_counter;

struct work_stealing_thread_pool_fixture : ::testing::Test
{
    work_stealing_thread_pool_fixture()
        : pool(nOSThreads)
		, exec( pool )
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

	using task_system = stk::thread::task_system<mc_queue_traits>; 
	static const std::size_t nTimingRuns = 200;
    stk::thread::work_stealing_thread_pool<mc_queue_traits> pool;
    task_system	exec;

	using task = task_system::task;
};

TEST_F( work_stealing_thread_pool_fixture, threading_threshold )
{
	using namespace std::chrono;
	std::vector<microseconds> durations = { 10us, 20us, 50us, 100us, 200us, 500us, 1000us };

	std::size_t nInvokations = 10000;
	std::size_t npartitions = pool.number_threads();
	std::cout << "\n=== Threading Threshold Test (" << nOSThreads << " threads) ===\n";

	for( auto dur : durations )
	{
		auto job = [dur]( int i ) { stk::synthetic_work( dur ); };
		auto single_duration = stk::time_execution( [&]()
			{ 
				for (std::size_t i = 0; i < nInvokations; ++i)
					job(i); 
			} );

		auto multi_duration = stk::time_execution( [&]() { pool.parallel_apply( nInvokations, job, npartitions ); } );
		double speedup = single_duration.count() / multi_duration.count();
		std::cout << dur.count() << "us per job: "
				  << "single=" << single_duration.count()
				  << "s multi=" << multi_duration.count()
				  << "s speedup=" << speedup << "x\n";
	}

	std::cout << "=============================================\n";
}

TEST_F( work_stealing_thread_pool_fixture, concurrent_flat_map_contention_vs_sequential )
{
	using Map = boost::unordered::concurrent_flat_map<int, int>;
	constexpr std::size_t nOps = 1'000'000;
	constexpr std::size_t nKeys = 1024; // small -> deliberate contention
	const std::size_t nThreads = pool.number_threads();

	std::cout << "\n=== concurrent_flat_map contention vs sequential ===\n";

	// Sequential baseline
	{
		Map  cmap;
		auto dur = stk::time_execution( [&]()
			{
            for (std::size_t i = 0; i < nOps; ++i)
                cmap.insert_or_assign(static_cast<int>(i % nKeys), static_cast<int>(i)); } );
		std::cout << "Sequential: " << dur.count() << "s\n";
	}

	// Parallel version using the pool
	{
		Map  cmap;
		auto job = [&]( std::size_t tid )
		{
			for( std::size_t i = 0; i < nOps / nThreads; ++i )
				cmap.insert_or_assign( static_cast<int>( i % nKeys ), static_cast<int>( i + tid * nOps ) );
		};

		auto dur = stk::time_execution( [&]()
			{ pool.parallel_apply( nThreads, job, nThreads ); } );
		std::cout << "Concurrent: " << dur.count() << "s\n";
	}

	for( std::size_t nKeys : { 1024, 16384, 1'000'000 } )
	{
		Map  cmap;
		auto dur = stk::time_execution( [&]()
			{ pool.parallel_apply( nThreads, [&]( std::size_t tid )
				  {
            for (std::size_t i = 0; i < nOps / nThreads; ++i)
                cmap.insert_or_assign(int(i % nKeys), int(i + tid * nOps)); },
				  nThreads ); } );
		std::cout << "nKeys=" << nKeys << " -> " << dur.count() << "s\n";
	}

	std::cout << "===============================================\n";
}

TEST_F( work_stealing_thread_pool_fixture, concurrent_node_map_contention_vs_sequential )
{
	using Map = boost::unordered::concurrent_node_map<int, int>;
	constexpr std::size_t nOps = 1'000'000;
	constexpr std::size_t nKeys = 1024; // small -> deliberate contention
	const std::size_t     nThreads = pool.number_threads();

	std::cout << "\n=== concurrent_node_map contention vs sequential ===\n";

	// Sequential baseline
	{
		Map  cmap;
		auto dur = stk::time_execution( [&]()
			{
            for (std::size_t i = 0; i < nOps; ++i)
                cmap.insert_or_assign(static_cast<int>(i % nKeys), static_cast<int>(i)); } );
		std::cout << "Sequential: " << dur.count() << "s\n";
	}

	// Parallel version using the pool
	{
		Map  cmap;
		auto job = [&]( std::size_t tid )
		{
			for( std::size_t i = 0; i < nOps / nThreads; ++i )
				cmap.insert_or_assign( static_cast<int>( i % nKeys ), static_cast<int>( i + tid * nOps ) );
		};

		auto dur = stk::time_execution( [&]()
			{ pool.parallel_apply( nThreads, job, nThreads ); } );
		std::cout << "Concurrent: " << dur.count() << "s\n";
	}

	// Scaling test with varying key-space sizes
	for( std::size_t nKeys : { 1024, 16384, 1'000'000 } )
	{
		Map  cmap;
		auto dur = stk::time_execution( [&]()
			{ pool.parallel_apply( nThreads, [&]( std::size_t tid )
				  {
                for (std::size_t i = 0; i < nOps / nThreads; ++i)
                    cmap.insert_or_assign(int(i % nKeys), int(i + tid * nOps)); },
				  nThreads ); } );
		std::cout << "nKeys=" << nKeys << " -> " << dur.count() << "s\n";
	}

	std::cout << "===============================================\n";
}

TEST_F( work_stealing_thread_pool_fixture, concurrent_maps_read_heavy_mixture )
{
	constexpr std::size_t nOps = 1'000'000;
	constexpr std::size_t nKeys = 16'384;
	constexpr double      writeRatio = 0.1; // 10% writes
	const std::size_t     nThreads = pool.number_threads();

	auto run_test = [&]( auto&& MapType, const char* name )
	{
		using Map = std::decay_t<decltype( MapType )>;
		Map cmap;
		cmap.reserve( nKeys );

		// prefill
		for( std::size_t i = 0; i < nKeys; ++i )
			cmap.insert_or_assign( static_cast<int>( i ), static_cast<int>( i * 2 ) );

		auto dur = stk::time_execution( [&]()
			{ pool.parallel_apply( nThreads, [&]( std::size_t tid )
				  {
                std::mt19937_64 rng(tid + 1);
                std::uniform_real_distribution<double> dist(0.0, 1.0);
                for (std::size_t i = 0; i < nOps / nThreads; ++i)
                {
                    int key = static_cast<int>(i % nKeys);
                    if (dist(rng) < writeRatio)
                        cmap.insert_or_assign(key, int(i + tid * nOps));
                    else
                    {
                        if constexpr (requires { cmap.visit(key, [](auto const&){}); })
                        {
                            cmap.visit(key, [](auto const& kv) {
                                [[maybe_unused]] auto v = kv.second;
                            });
                        }
                        else
                        {
                            cmap.visit(key, [](auto const& kv) {
                                [[maybe_unused]] auto v = kv.second;
                            });
                        }
                    }
                } },
				  nThreads ); } );

		std::cout << name << ": " << dur.count() << "s\n";
	};

	std::cout << "\n=== concurrent_flat_map vs node_map (read-heavy) ===\n";
	run_test( boost::unordered::concurrent_flat_map<int, int>{}, "flat_map" );
	run_test( boost::unordered::concurrent_node_map<int, int>{}, "node_map" );
	std::cout << "====================================================\n";
}

TEST_F( work_stealing_thread_pool_fixture, DependencyGraph )
{
	using namespace stk::thread;
	auto*              a = exec.submit( [] { printf( "Load\n" ); } );
	std::vector<task*> mids;
	for( int i = 0; i < 8; ++i )
		mids.push_back( exec.submit_after( [i] { printf( "Compute %d\n", i ); }, a ) );
	auto* join = exec.submit_after( mids, [] { printf( "Compose\n" ); } );
	exec.wait( join );

	EXPECT_TRUE( !pool.has_outstanding_tasks() );
}

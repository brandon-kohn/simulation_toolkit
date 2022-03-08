#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <stk/utility/memory_pool.hpp>

TEST(memory_pool_test_suite, construct)
{
	using namespace stk;
	auto sut = memory_pool<int>{};
	std::vector<int*> nodes;
}

TEST( memory_pool_test_suite, allocate_and_deallocate )
{
	using namespace stk;
	using pool_t = memory_pool<int, constant_growth_policy<3>>;
	auto              sut = pool_t{};
	std::vector<int*> nodes;

	auto* v0 = sut.allocate();
	pool_t::construct( v0, 69 );
	EXPECT_EQ( 69, *v0 );
	auto* v1 = sut.allocate();
	pool_t::construct( v1, 70 );
	EXPECT_EQ( 70, *v1 );
	auto* v2 = sut.allocate();
	pool_t::construct( v2, 71 );
	EXPECT_EQ( 71, *v2 );
	sut.deallocate( v0 );
	sut.deallocate( v1 );
	auto* v3 = sut.allocate();
	pool_t::construct( v3, 73 );
	EXPECT_EQ( 73, *v3 );
	sut.deallocate( v2 );
	sut.deallocate( v3 );

	auto* v4 = sut.allocate();
	pool_t::construct( v4, 74 );
	EXPECT_EQ( 74, *v4 );
}

#include <stk/thread/work_stealing_thread_pool.hpp>
#include <stk/thread/concurrentqueue.h>
#include <stk/thread/concurrentqueue_queue_info_no_tokens.h>
#include <geometrix/utility/scope_timer.ipp>

using mc_queue_traits = moodycamel_concurrent_queue_traits_no_tokens;

std::size_t nAllocations = 1000000;

TEST( memory_pool_test_suite, cross_thread_bench_alloc_deallocate )
{
	using namespace stk::thread;
	std::size_t nOSThreads = std::thread::hardware_concurrency() - 1;
	using pool_t = work_stealing_thread_pool<mc_queue_traits>;
	pool_t pool( nOSThreads );

	using mpool_t = stk::memory_pool<int, stk::geometric_growth_policy<10>>;
	auto sut = mpool_t{};

	using future_t = pool_t::future<void>;
	std::vector<future_t> futures;
	futures.reserve( nAllocations );
	{
		for( size_t i = 0; i < nAllocations; ++i )
		{
			futures.emplace_back( pool.send( i++ % pool.number_threads(),
				[&]()
				{
					std::array<int*, 1000> allocs;
					for( auto& ind : allocs )
					{
						ind = sut.allocate();
						mpool_t::construct( ind, i );
					}
					for( auto& ind : allocs ) 
					{
						sut.deallocate( ind );
					}
				} ) );
		}

		boost::for_each( futures, []( const future_t& f ) { f.wait(); } );

		EXPECT_EQ( sut.size_elements(), sut.size_free() );
	}
}
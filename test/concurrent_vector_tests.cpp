//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <stk/thread/active_object.hpp>
#include <stk/thread/thread_pool.hpp>
#include <stk/thread/tiny_atomic_spin_lock.hpp>
#include <stk/container/fine_locked_hash_map.hpp>
#include <stk/thread/boost_thread_kernel.hpp>
#include <boost/thread/futures/wait_for_all.hpp>

#include <boost/context/stack_traits.hpp>

#include <geometrix/utility/scope_timer.ipp>

#include <stk/thread/concurrentqueue.h>
#include <stk/thread/concurrentqueue_queue_info.h>

#include <thread>
#include <chrono>
#include <exception>
#include <iostream>
#include <geometrix/utility/scope_timer.ipp>
#include <stk/container/experimental/concurrent_vector.hpp>

TEST(lock_free_concurrent_vector, construct)
{
	using namespace stk;
	concurrent_vector<int> v;
	EXPECT_EQ(0, v.size());
	EXPECT_GE(2, v.capacity());
}

TEST(lock_free_concurrent_vector, construct_reserve)
{
	using namespace stk;
	concurrent_vector<int> v(reserve_arg, 10);
	EXPECT_EQ(0, v.size());
	EXPECT_LE(10, v.capacity());//! cumulative geometrix = 2 + 4 + 8
}

TEST(lock_free_concurrent_vector, construct_generate)
{
	using ::testing::ElementsAre;
	using namespace stk;
	int count = 2;
	auto generator = [&count]() { return count++; };
	concurrent_vector<int> v(generator_arg, 10, generator);
	EXPECT_EQ(10, v.size());
	EXPECT_THAT(v, ElementsAre(2, 3, 4, 5, 6, 7, 8, 9, 10, 11));
}

TEST(lock_free_concurrent_vector, construct_iterators)
{
	using ::testing::ElementsAre;
	using namespace stk;
	std::vector<int> vec = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
	concurrent_vector<int> v(vec.begin(), vec.end());
	EXPECT_EQ(10, v.size());
	EXPECT_THAT(v, ElementsAre(2, 3, 4, 5, 6, 7, 8, 9, 10, 11));
}

TEST(lock_free_concurrent_vector, construct_initializer_list)
{
	using ::testing::ElementsAre;
	using namespace stk;
	concurrent_vector<int> v = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
	EXPECT_EQ(10, v.size());
	EXPECT_THAT(v, ElementsAre(2, 3, 4, 5, 6, 7, 8, 9, 10, 11));
}

TEST(lock_free_concurrent_vector, push_back)
{
	using namespace stk;
	concurrent_vector<int> v;
	v.push_back(10);
	EXPECT_EQ(1, v.size());
}

TEST(lock_free_concurrent_vector, pop_back)
{
	using namespace stk;
	concurrent_vector<int> v;
	v.push_back(10);
	int val;
	auto r = v.pop_back(val);
	EXPECT_EQ(10, val);
	EXPECT_EQ(0, v.size());
}

TEST(lock_free_concurrent_vector, push_back_10)
{
	using namespace stk;
	concurrent_vector<int> v;
	for(auto i = 1UL; i < 11; ++i)
		v.push_back(i);
	EXPECT_EQ(10, v.size());
	EXPECT_EQ(14, v.capacity());
	for(auto i = 0UL; i < 10; ++i)
		EXPECT_EQ(i+1, v[i]);

}

TEST(lock_free_concurrent_vector, pop_back_on_empty)
{
	using namespace stk;
	concurrent_vector<int> v;
	int val;
	EXPECT_FALSE(v.pop_back(val));
}

TEST(lock_free_concurrent_vector, iteration)
{
	using ::testing::ElementsAre;
	using namespace stk;
	concurrent_vector<int> v;
	EXPECT_EQ(v.begin(), v.end());
	EXPECT_EQ(v.cbegin(), v.cend());
	EXPECT_EQ(v.cbegin(), v.end());

	for(auto i = 1UL; i < 11; ++i)
		v.push_back(i);

	EXPECT_THAT(v, ElementsAre(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
}

TEST(lock_free_concurrent_vector, iteration_with_pops_to_premature_end)
{
	using ::testing::ElementsAre;
	using namespace stk;
	concurrent_vector<int> v;

	std::vector<int> r;
	for (auto i = 1UL; i < 11; ++i)
	{
		v.push_back(i);
	}

	for(auto it = v.begin(); it != v.end(); ++it)
	{
		v.pop_back();
		r.push_back(*it);
	}

	EXPECT_THAT(r, ElementsAre(1, 2, 3, 4, 5));
}

TEST(lock_free_concurrent_vector, iterations_up_and_back)
{
	using ::testing::ElementsAre;
	using namespace stk;
	concurrent_vector<int> v;

	std::vector<int> r;
	for (auto i = 1UL; i < 11; ++i)
	{
		v.push_back(i);
	}

	auto it = v.begin();
	for (; it != v.end(); ++it);
	for (; it != v.begin(); --it);

	EXPECT_EQ(it, v.begin());
}

#include <stk/thread/work_stealing_thread_pool.hpp>
#include <stk/thread/concurrentqueue.h>
TEST(lock_free_concurrent_vector, bash_concurrency_test)
{
	using namespace stk;
	using namespace stk::thread;

	work_stealing_thread_pool<moodycamel_concurrent_queue_traits> pool(10);

	auto nItems = 10000UL;

	concurrent_vector<int> v;
	for (auto i = 0UL; i < 20; ++i)
	{
		{
			GEOMETRIX_MEASURE_SCOPE_TIME("concurrent_vector");
			pool.parallel_apply(nItems, [&v](int q) {
				v.push_back(q);
				v.pop_back();
				v.push_back(q);
			});
			v.quiesce();
		}
		EXPECT_EQ((i+1) * nItems, v.size());
	}
		
	EXPECT_EQ(20 * nItems, v.size());
}

TEST(lock_free_concurrent_vector, bash_seq_concurrency_test)
{
	using namespace stk;
	using namespace stk::thread;

	work_stealing_thread_pool<moodycamel_concurrent_queue_traits> pool(10);

	auto nItems = 10000UL;

	std::mutex mtx;
	std::vector<int> v;
	for (auto i = 0UL; i < 20; ++i)
	{
		{
			GEOMETRIX_MEASURE_SCOPE_TIME("mutexed_vector");
			pool.parallel_apply(nItems, [&mtx, &v](int q) {
				{
					auto lk = std::unique_lock<std::mutex>{ mtx };
					v.push_back(q);
				}
				{
					auto lk = std::unique_lock<std::mutex>{ mtx };
					v.pop_back();
				}
				{
					auto lk = std::unique_lock<std::mutex>{ mtx };
					v.push_back(q);
				}
			});
		}
		EXPECT_EQ((i+1) * nItems, v.size());
	}
		
	EXPECT_EQ(20 * nItems, v.size());
}

#include <stk/container/ref_count_memory_reclaimer.hpp>
TEST(ref_count_memory_reclaimer_suite, bash_reclaimer)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;

	std::atomic<std::uint32_t> reclaimed(0UL);
	auto nItems = 100000;
	work_stealing_thread_pool<moodycamel_concurrent_queue_traits> pool;
	ref_count_memory_reclaimer sut;

	pool.parallel_apply(nItems, [&reclaimed, &sut](int) {
		sut.add_checkout();
		sut.add([&reclaimed]() { ++reclaimed; });
		sut.remove_checkout();
	});

	EXPECT_EQ(nItems, reclaimed);
}

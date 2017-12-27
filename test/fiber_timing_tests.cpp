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
#include <stk/thread/work_stealing_thread_pool.hpp>
#include <stk/thread/fiber_pool.hpp> //-> requires C++11 with constexpr/noexcept etc. vs140 etc.
#include <stk/thread/tiny_atomic_spin_lock.hpp>
#include <stk/container/fine_locked_hash_map.hpp>
#include <junction/ConcurrentMap_Leapfrog.h>
#include <junction/ConcurrentMap_Linear.h>
#include <junction/ConcurrentMap_Grampa.h>
#include <junction/ConcurrentMap_Crude.h>
#include <boost/thread/futures/wait_for_all.hpp>

#include <boost/context/stack_traits.hpp>

#include <geometrix/utility/scope_timer.ipp>

#include <stk/thread/concurrentqueue.h>

#include <thread>
#include <chrono>
#include <exception>
#include <iostream>
#include <unordered_map>

std::size_t nsubwork = 10;
STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(std::uint32_t);//! for work_stealing_thread_pool.

//! Pull this symbol in so gcc doesn't optimize away.
auto default_stack_size = boost::context::stack_traits::default_size();

template <typename Mutex>
void bash_map_sequential(const char* name)
{
	using namespace ::testing;
	using namespace stk;

	fine_locked_hash_map<int, int, std::hash<int>, Mutex> m(200000);

	auto nItems = 10000;
	for (auto i = 0; i < nItems; ++i)
	{
		m.add(i, i * 10);
	}

	{
		GEOMETRIX_MEASURE_SCOPE_TIME(name);
		for (unsigned i = 0; i < 100000; ++i) {
			for (int q = 0; q < nsubwork; ++q)
			{
				m.add_or_update(i, i * 20);
				m.remove(i);
				m.add_or_update(i, i * 20);
			}
		}
	}

	for (auto i = 0; i < 100000; ++i)
	{
		auto r = m.find(i);
		EXPECT_TRUE(r);
		EXPECT_EQ(i * 20, *r);
	}
}

template <typename Mutex, typename Pool>
void bash_map(Pool& pool, const char* name)
{
	using namespace ::testing;
	using namespace stk;

	fine_locked_hash_map<int, int, std::hash<int>, Mutex> m(200000);

	auto nItems = 10000;
	for(auto i = 0; i < nItems; ++i)
	{
		m.add(i, i * 10);
	}

	using future_t = typename Pool::template future<void>;
	std::vector<future_t> fs;
	fs.reserve(100000);
	{
		GEOMETRIX_MEASURE_SCOPE_TIME(name);
		for (unsigned i = 0; i < 100000; ++i) {
			fs.emplace_back(pool.send([&m, i]() -> void 
			{
				for (int q = 0; q < nsubwork; ++q)
				{
					m.add_or_update(i, i * 20);
					m.remove(i);
					m.add_or_update(i, i * 20);
				}
			}));
		}
		boost::for_each(fs, [](const future_t& f) { f.wait(); });
	}
	
	boost::for_each(fs, []( future_t& f) { EXPECT_NO_THROW(f.get()); });
	
	for (auto i = 0; i < 100000; ++i) 
	{
		auto r = m.find(i);
		EXPECT_TRUE(r);
		EXPECT_EQ(i * 20, *r);
	}
}

template <typename Pool>
void bash_junction_map(Pool& pool, const char* name)
{
	using namespace ::testing;
	using namespace stk;

	junction::ConcurrentMap_Leapfrog<int, int> m;

	auto nItems = 10000;
	for (auto i = 2; i < nItems + 2; ++i)
	{
		m.assign(i, i * 10);
	}

	using future_t = typename Pool::template future<void>;
	std::vector<future_t> fs;
	fs.reserve(100000);
	{
		GEOMETRIX_MEASURE_SCOPE_TIME(name);
		for (unsigned i = 2; i < 100000 + 2; ++i) {
			fs.emplace_back(pool.send([&m, i]() -> void
			{
				for (int q = 0; q < nsubwork; ++q)
				{
					m.assign(i, i * 20);
					m.erase(i);
					m.assign(i, i * 20);
				}
			}));
		}
		boost::for_each(fs, [](const future_t& f) { f.wait(); });
	}

	for (auto i = 2; i < 100000 + 2; ++i)
	{
		auto r = m.find(i);
		EXPECT_EQ(i * 20, r.getValue());
	}
}

template <typename Mutex, typename Pool>
void bash_synchronized_map(Pool& pool, const char* name)
{
	using namespace ::testing;
	using namespace stk;

	std::unordered_map<int, int> m;

	Mutex mtx;

	auto nItems = 10000;
	for (auto i = 0; i < nItems; ++i)
	{
		m.insert(std::make_pair(i, i * 10));
	}

	using future_t = typename Pool::template future<void>;
	std::vector<future_t> fs;
	fs.reserve(100000);
	{
		GEOMETRIX_MEASURE_SCOPE_TIME(name);
		for (unsigned i = 0; i < 100000; ++i) {
			fs.emplace_back(pool.send([&m, &mtx, i]() -> void
			{
				for (int q = 0; q < nsubwork; ++q)
				{
					auto lk = std::unique_lock<Mutex>{ mtx };
					m[i] = i * 20;
					m.erase(i);
					m[i] = i * 20;
				}
			}));
		}
		boost::for_each(fs, [](const future_t& f) { f.wait(); });
	}

	for (auto i = 0; i < 100000; ++i)
	{
		auto it = m.find(i);
		ASSERT_TRUE(it != m.end());
		EXPECT_EQ(i * 20, it->second);
	}
}


int nTimingRuns = 20;
// TEST(timing, fibers_fibers_mutex)
// {
// 	using namespace ::testing;
// 	using namespace stk;
// 	using namespace stk::thread;
// 	auto alloc = boost::fibers::fixedsize_stack{ 64 * 1024 };
// 	fiber_pool<> fibers{ 10, alloc };
// 	for (int i = 0; i < 1/*nTimingRuns*/; ++i)
// 	{
// 		bash_map<boost::fibers::mutex>(fibers, "fiber pool/fibers::mutex");
// 	}
// 
// 	EXPECT_TRUE(true);
// }

TEST(timing, fibers_moodycamel_concurrentQ)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	auto alloc = boost::fibers::fixedsize_stack{ 64 * 1024 };
	fiber_pool<boost::fibers::fixedsize_stack, moodycamel_concurrent_queue_traits> fibers{ 10, alloc };
	for (int i = 0; i < nTimingRuns; ++i)
	{
		bash_map<boost::fibers::mutex>(fibers, "fiber pool moody-concurrent/fibers::mutex");
	}

	EXPECT_TRUE(true);
}

TEST(timing, fibers_moodycamel_concurrentQ_tiny_atomic_spinlock_eager_fiber_yield_5000)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	auto alloc = boost::fibers::fixedsize_stack{ 64 * 1024 };
	fiber_pool<boost::fibers::fixedsize_stack, moodycamel_concurrent_queue_traits> fibers{ 10, alloc };
	for (int i = 0; i < nTimingRuns; ++i)
	{
		bash_map<tiny_atomic_spin_lock<eager_fiber_yield_wait<5000>>>(fibers, "fiber pool moody-concurrent/tiny_atomic_spin_lock<eager_yield_wait<5000>>");
	}

	EXPECT_TRUE(true);
}

TEST(timing, fibers_moodycamel_concurrentQ_atomic_spinlock_eager_fiber_yield_5000)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	auto alloc = boost::fibers::fixedsize_stack{ 64 * 1024 };
	fiber_pool<boost::fibers::fixedsize_stack, moodycamel_concurrent_queue_traits> fibers{ 10, alloc };
	for (int i = 0; i < nTimingRuns; ++i)
	{
		bash_map<atomic_spin_lock<eager_fiber_yield_wait<5000>>>(fibers, "fiber pool moody-concurrent/atomic_spin_lock<eager_yield_wait<5000>>");
	}

	EXPECT_TRUE(true);
}

TEST(timing, threads)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;

	thread_pool<> threads(5);
	for (int i = 0; i < nTimingRuns; ++i)
	{
		bash_map<std::mutex>(threads, "thread pool/std::mutex");
	}

	EXPECT_TRUE(true);
}

TEST(timing, threads_moodycamel_std_mutex)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;

	thread_pool<moodycamel_concurrent_queue_traits> threads(5);
	for (int i = 0; i < nTimingRuns; ++i)
	{
		bash_map<std::mutex>(threads, "thread pool moody-camel/std::mutex");
	}

	EXPECT_TRUE(true);
}

TEST(timing, threads_moodycamel_atomic_spinlock_eager_5000)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;

	thread_pool<moodycamel_concurrent_queue_traits> threads(5);
	for (int i = 0; i < nTimingRuns; ++i)
	{
		bash_map<atomic_spin_lock<eager_boost_thread_yield_wait<5000>>>(threads, "thread pool moody-camel/atomic_spinlock_eager_5000");
	}

	EXPECT_TRUE(true);
}

TEST(timing, threads_atomic_spinlock_eager_5000)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;

	thread_pool<> threads(5);
	for (int i = 0; i < nTimingRuns; ++i)
	{
		bash_map<atomic_spin_lock<eager_boost_thread_yield_wait<5000>>>(threads, "thread pool/atomic_spin_lock<eager_yield_wait<5000>>");
	}
	
	EXPECT_TRUE(true);
}

TEST(timing, work_stealing_thread_pool_moodycamel_concurrentQ_tiny_atomic_spinlock_eager_fiber_yield_5000)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	work_stealing_thread_pool<moodycamel_concurrent_queue_traits> pool{5};
	for (int i = 0; i < nTimingRuns; ++i)
	{
		bash_map<tiny_atomic_spin_lock<eager_boost_thread_yield_wait<5000>>>(pool, "work_stealing_thread_pool moody-concurrent/tiny_atomic_spin_lock<eager_yield_wait<5000>>");
	}

	EXPECT_TRUE(true);
}

TEST(timing, work_stealing_thread_pool_moodycamel_concurrentQ_bash_junction)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	work_stealing_thread_pool<moodycamel_concurrent_queue_traits> pool{ 5 };
	for (int i = 0; i < nTimingRuns * 10; ++i)
	{
		bash_junction_map(pool, "work_stealing_thread_pool moody-concurrent/bash_junction_map");
	}

	EXPECT_TRUE(true);
}

class null_mutex
{
public:

	null_mutex() = default;

	void lock()
	{
		
	}

	bool try_lock()
	{
		return true;
	}

	void unlock()
	{
	}
};

TEST(timing, bash_map_sequential_null_mutex)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;

	for (int i = 0; i < nTimingRuns; ++i)
	{
		bash_map_sequential<null_mutex>("sequential/null mutex");
	}

	EXPECT_TRUE(true);
}

TEST(timing, bash_map_sequential_std_mutex)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;

	for (int i = 0; i < nTimingRuns; ++i)
	{
		bash_map_sequential<std::mutex>("sequential/std::mutex");
	}

	EXPECT_TRUE(true);
}

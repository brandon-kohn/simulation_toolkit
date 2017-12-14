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
#include <stk/thread/fiber_pool.hpp> //-> requires C++11 with constexpr/noexcept etc. vs140 etc.
#include <stk/thread/tiny_atomic_spin_lock.hpp>
#include <stk/container/fine_locked_hash_map.hpp>
#include <boost/thread/futures/wait_for_all.hpp>

#include <geometrix/utility/scope_timer.ipp>

#include <stk/thread/concurrentqueue.h>

#include <thread>
#include <chrono>
#include <exception>
#include <iostream>

TEST(fine_locked_hash_map, construct)
{
	using namespace ::testing;
	using namespace stk;

	static_assert(sizeof(stk::thread::tiny_atomic_spin_lock<>) == 1, "size should be one byte.");

	fine_locked_hash_map<int, int> m;
}

TEST(fine_locked_hash_map, add_item)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	
	fine_locked_hash_map<int, int> m;

	m.add_or_update(10, 20);
	m.add_or_update(20, 30);
	m.add_or_update(30, 40);

	auto r = m.find(10);
	ASSERT_TRUE(r);
	EXPECT_EQ(20, *r);

	r = m.find(20);
	ASSERT_TRUE(r);
	EXPECT_EQ(30, *r);

	r = m.find(30);
	ASSERT_TRUE(r);
	EXPECT_EQ(40, *r);
}

TEST(fine_locked_hash_map, add_update_item)
{
	using namespace ::testing;
	using namespace stk;

	fine_locked_hash_map<int, int> m;

	m.add_or_update(10, 20);
	auto r = m.add(10, 20);

	EXPECT_FALSE(r);
}

/* - Not implemented yet.
#include <stk/thread/make_ready_future.hpp>
TEST(fiber_make_ready_future, construct)
{
	using namespace ::testing;
	std::vector<int> values = { 1, 2, 3, 4, 5, 6 };

	auto f = boost::fibers::make_ready_future(values);
	EXPECT_THAT(values, ElementsAre(1, 2, 3, 4, 5, 6));

	EXPECT_TRUE(f.valid());

	auto v = f.get();
	
	EXPECT_THAT(v, ElementsAre(1, 2, 3, 4, 5, 6));
}
*/

TEST(active_object_test, construct)
{
    using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	active_object<> obj;

	std::atomic<bool> isRun = false;
	auto r = obj.send([&]() { return isRun = true; });
	
	EXPECT_TRUE(r.get() && isRun);
}

TEST(active_object_test, exception)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	active_object<> obj;

	auto r = obj.send([&]() -> void
	{
		throw std::logic_error("");
	});

	r.wait();
	EXPECT_TRUE(r.has_exception());
	EXPECT_THROW(r.get(), std::logic_error);
}

TEST(thread_pool_test, construct)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	thread_pool<> obj;

	static_assert(sizeof(std::atomic<std::uint8_t>) == 1, "shit!");

	std::atomic<bool> isRun = false;
	auto r = boost::when_all( obj.send([&]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, obj.send([&]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, obj.send([&]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, obj.send([&]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, obj.send([&]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, obj.send([&]() -> bool { std::this_thread::sleep_for(std::chrono::milliseconds(10)); return isRun = true; }));

	r.wait();

	EXPECT_TRUE(isRun);
}

// TEST(fiber_pool_test, construct)
// {
// 	using namespace ::testing;
// 	using namespace stk;
// 	using namespace stk::thread;
// 	
// 	auto alloc = boost::fibers::fixedsize_stack{64 * 1024};
// 	{
// 		fiber_pool<> obj{ 10, alloc };;
// 
// 		std::atomic<bool> isRun = false;
// 
// 		std::vector<boost::fibers::future<void>> r;
// 		r.emplace_back(obj.send([&]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(10)); }));
// 		r.emplace_back(obj.send([&]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(10)); }));
// 		r.emplace_back(obj.send([&]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(10)); }));
// 		r.emplace_back(obj.send([&]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(10)); }));
// 		r.emplace_back(obj.send([&]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(10)); }));
// 		r.emplace_back(obj.send([&]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(10)); isRun = true; }));
// 
// 		boost::for_each(r, [](const boost::fibers::future<void>& f) { f.wait(); });
// 
// 		EXPECT_TRUE(isRun);
// 	}
// }

std::size_t nsubwork = 10;

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

	using future_t = typename Pool::future<void>;
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

	for (auto i = 0; i < 100000; ++i) 
	{
		auto r = m.find(i);
		EXPECT_TRUE(r);
		EXPECT_EQ(i * 20, *r);
	}
}

std::size_t nTimingRuns = 20;
// TEST(timing, fibers_tiny_spin_yield_wait)
// {
// 	using namespace ::testing;
// 	using namespace stk;
// 	using namespace stk::thread;
// 
// 	auto alloc = boost::fibers::fixedsize_stack{ 64 * 1024 };
// 	fiber_pool<> fibers{ 10, alloc };
// 
// 	for (int i = 0; i < nTimingRuns; ++i)
// 	{
// 		bash_map<tiny_atomic_spin_lock<fiber_yield_wait>>(fibers, "fiber pool/yield_wait");
// 	}
// 
// 	EXPECT_TRUE(true);
// }

// TEST(timing, fibers_tiny_spin_eager_yield_wait)
// {
// 	using namespace ::testing;
// 	using namespace stk;
// 	using namespace stk::thread;
// 
// 	auto alloc = boost::fibers::fixedsize_stack{ 64 * 1024 };
// 	fiber_pool<> fibers{ 10, alloc };
// 	
// 	for (int i = 0; i < nTimingRuns; ++i)
// 	{
// 		bash_map<tiny_atomic_spin_lock<eager_fiber_yield_wait<5000>>>(fibers, "fiber pool/eager_yield_wait_5000");
// 	}
// 
// 	EXPECT_TRUE(true);
// }

// TEST(timing, fibers_fibers_mutex)
// {
// 	using namespace ::testing;
// 	using namespace stk;
// 	using namespace stk::thread;
// 	auto alloc = boost::fibers::fixedsize_stack{ 64 * 1024 };
// 	fiber_pool<> fibers{ 10, alloc };
// 	for (int i = 0; i < nTimingRuns; ++i)
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
		bash_map<std::mutex>(threads, "thread pool moody-camel/atomic_spinlock_eager_5000");
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
		bash_map<atomic_spin_lock<eager_fiber_yield_wait<5000>>>(threads, "thread pool/atomic_spin_lock<eager_yield_wait<5000>>");
	}

	EXPECT_TRUE(true);
}

#include <stk/thread/fiber_thread_system.hpp>
template <typename Mutex>
void bash_map_fibers_async(const char* name)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;

	fiber_thread_system<> fts(boost::fibers::fixedsize_stack{ 64 * 1024 });

	fine_locked_hash_map<int, int, std::hash<int>, Mutex> m(200000);

	auto nItems = 10000;
	for (auto i = 0; i < nItems; ++i)
	{
		m.add(i, i * 10);
	}

	using future_t = boost::fibers::future<void>;
	std::vector<future_t> fs;
	fs.reserve(100000);
	{
		GEOMETRIX_MEASURE_SCOPE_TIME(name);
		for (unsigned i = 0; i < 100000; ++i) {

			auto f = fts.async([&m, i]() -> void
			{
				for (int q = 0; q < nsubwork; ++q)
				{
					m.add_or_update(i, i * 20);
					m.remove(i);
					m.add_or_update(i, i * 20);
				}
			});

			fs.emplace_back(std::move(f));
		}
		boost::for_each(fs, [](const future_t& f) { f.wait(); });
	}

	for (auto i = 0; i < 100000; ++i)
	{
		auto r = m.find(i);
		EXPECT_TRUE(r);
		EXPECT_EQ(i * 20, *r);
	}
}

// TEST(timing, bash_map_bf_async_bf_mutex)
// {
// 	using namespace ::testing;
// 	using namespace stk;
// 	using namespace stk::thread;
// 
// 	for (int i = 0; i < nTimingRuns; ++i)
// 	{
// 		bash_map_fibers_async<boost::fibers::mutex>("bf::async/bf::mutex");
// 	}
// 
// 	EXPECT_TRUE(true);
// }

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

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

#include <boost/context/stack_traits.hpp>

#include <geometrix/utility/scope_timer.ipp>

#include <stk/thread/concurrentqueue.h>
#include <stk/thread/pool_work_stealing.hpp>

#include <thread>
#include <chrono>
#include <exception>
#include <iostream>

TEST(fine_locked_hash_map, construct)
{
	//! Had to put this verify in to stop this reference from being dropped on gcc in linux.
	GEOMETRIX_VERIFY(boost::context::stack_traits::default_size() > 0);
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

	std::atomic<bool> isRun(false);
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

#include <stk/thread/boost_fiber_traits.hpp>
TEST(active_object_test, construct_fiber_active_object)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;

	active_object<boost_fiber_traits> obj(boost_fiber_creation_policy<>(boost::fibers::fixedsize_stack{ 64 * 1024 }));

	std::atomic<bool> isRun(false);
	auto r = obj.send([&]() { return isRun = true; });

	EXPECT_TRUE(r.get() && isRun);
}

TEST(thread_pool_test, construct)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	thread_pool<> obj;

	std::atomic<bool> isRun(false);
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

	for (auto i = 0; i < 100000; ++i) 
	{
		auto r = m.find(i);
		EXPECT_TRUE(r);
		EXPECT_EQ(i * 20, *r);
	}
}

std::size_t nTimingRuns = 20;
TEST(timing, fibers_fibers_mutex)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	auto alloc = boost::fibers::fixedsize_stack{ 64 * 1024 };
	fiber_pool<> fibers{ 10, alloc };
	for (int i = 0; i < nTimingRuns; ++i)
	{
		bash_map<boost::fibers::mutex>(fibers, "fiber pool/fibers::mutex");
	}

	EXPECT_TRUE(true);
}

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

///////////////////////////////////////////////////////////////////////////////
using allocator_type = boost::fibers::fixedsize_stack;
std::uint64_t skynet(allocator_type& salloc, std::uint64_t num, std::uint64_t size, std::uint64_t div)
{
	if (size != 1)
	{
		size /= div;

		std::vector<boost::fibers::future<std::uint64_t>> results;
		results.reserve(div);

		for (std::uint64_t i = 0; i != div; ++i)
		{
			std::uint64_t sub_num = num + i * size;
			results.emplace_back(boost::fibers::async(boost::fibers::launch::dispatch, std::allocator_arg, salloc, skynet, std::ref(salloc), sub_num, size, div));
		}
		
		std::uint64_t sum = 0;
		for (auto& f : results)
			sum += f.get();

		return sum;
	}

	return num;
}

TEST(timing, boost_fibers_skynet_raw)
{
	using namespace stk;
	using namespace stk::thread;

	// Windows 10 and FreeBSD require a fiber stack of 8kb
	// otherwise the stack gets exhausted
	// stack requirements must be checked for other OS too
#if BOOST_OS_WINDOWS || BOOST_OS_BSD
	allocator_type salloc{ 2 * allocator_type::traits_type::page_size() };
#else
	allocator_type salloc{ allocator_type::traits_type::page_size() };
#endif

	std::vector<boost::intrusive_ptr<boost::fibers::algo::pool_work_stealing>> schedulers(std::thread::hardware_concurrency());
	work_stealing_fiber_scheduler_args args{ 0, &schedulers, false };
	fiber_thread_system<allocator_type, boost::fibers::algo::pool_work_stealing> fts(salloc, args);
	{
		auto f = fts.async([this, &salloc]() {
			GEOMETRIX_MEASURE_SCOPE_TIME("boost_fibers_skynet_raw");
			auto r = skynet(salloc, 0, 1000000, 10);
			EXPECT_EQ(499999500000, r);
		});

		f.wait();
	}
}

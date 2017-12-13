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
	{
		GEOMETRIX_MEASURE_SCOPE_TIME(name);
		for (unsigned i = 0; i < 100000; ++i) {
			fs.emplace_back(pool.send([&m, i]() -> void 
			{
				m.add_or_update(i, i * 20); 
				m.remove(i); 
				m.add_or_update(i, i * 20); 
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

std::size_t nTimingRuns = 10;
TEST(timing, fibers_tiny_spin_yield_wait)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;

	auto alloc = boost::fibers::fixedsize_stack{ 64 * 1024 };
	fiber_pool<> fibers{ 10, alloc };

	for (int i = 0; i < nTimingRuns; ++i)
	{
		bash_map<tiny_atomic_spin_lock<fiber_yield_wait>>(fibers, "fiber pool/yield_wait");
	}

	EXPECT_TRUE(true);
}


TEST(timing, fibers_tiny_spin_eager_yield_wait)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;

	auto alloc = boost::fibers::fixedsize_stack{ 64 * 1024 };
	fiber_pool<> fibers{ 10, alloc };
	
	for (int i = 0; i < nTimingRuns; ++i)
	{
		bash_map<tiny_atomic_spin_lock<eager_fiber_yield_wait<5000>>>(fibers, "fiber pool/eager_yield_wait_5000");
	}

	EXPECT_TRUE(true);
}

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

// static std::atomic< bool > fini{ false };
// 
// boost::fibers::future< int > fibonacci(int);
// 
// int fibonacci_(int n) {
// 	boost::this_fiber::yield();
// 	if (0 == n) {
// 		return 0;
// 	} else if (1 == n || 2 == n) {
// 		return 1;
// 	} else {
// 		boost::fibers::future< int > f1 = fibonacci(n - 1);
// 		boost::fibers::future< int > f2 = fibonacci(n - 2);
// 		return f1.get() + f2.get();
// 	}
// }
// 
// boost::fibers::future< int > fibonacci(int n) {
// 	boost::fibers::packaged_task< int() > pt(std::bind(fibonacci_, n));
// 	boost::fibers::future< int > f(pt.get_future());
// 	boost::fibers::fiber(boost::fibers::launch::dispatch, std::move(pt)).detach();
// 	return f;
// }
// 
// void thread(unsigned int max_idx, unsigned int idx, stk::thread::barrier * b) {
// 	boost::fibers::use_scheduling_algorithm< boost::fibers::algo::work_stealing >(max_idx);
// 	b->wait();
// 	while (!fini) {
// 		// To guarantee progress, we must ensure that
// 		// threads that have work to do are not unreasonably delayed by (thief) threads
// 		// which are idle except for task-stealing. 
// 		// This call yields the thief ’s processor to another thread, allowing
// 		// descheduled threads to regain a processor and make progress. 
// 		std::this_thread::yield();
// 		boost::this_fiber::yield();
// 	}
// }

// TEST(bloost, owk)
// {
// 	using namespace stk::thread;
// 	unsigned int max_idx = 6;
// 	boost::fibers::use_scheduling_algorithm< boost::fibers::algo::work_stealing >(max_idx);
// 	std::vector< std::thread > threads;
// 	barrier b(max_idx);
// 	// launch a couple threads to help process them
// 	for (unsigned int idx = 0; idx < max_idx-1; ++idx) {
// 		threads.push_back(std::thread(thread, max_idx, idx, &b));
// 	};
// 	b.wait();
// 	int n = 25;
// 	// main fiber computes fibonacci( n)
// 	// wait on result
// 	int result = fibonacci(n).get();
// 	//    BOOST_ASSERT( 55 == result);
// 	std::ostringstream buffer;
// 	buffer << "fibonacci(" << n << ") = " << result << '\n';
// 	std::cout << buffer.str() << std::flush;
// 	// set termination flag
// 	fini = true;
// 	// wait for threads to terminate
// 	for (std::thread & t : threads) {
// 		t.join();
// 	}
// 	std::cout << "done." << std::endl;
// }

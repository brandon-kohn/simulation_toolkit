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

#include <stk/thread/work_stealing_thread_pool.hpp>
STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(std::uint32_t);
TEST(work_stealing_thread_pool_test, construct)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	work_stealing_thread_pool<> obj;

	std::atomic<bool> isRun(false);
	auto r = boost::when_all(obj.send([&]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, obj.send([]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, obj.send([]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, obj.send([]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, obj.send([]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, obj.send([&]() -> bool { std::this_thread::sleep_for(std::chrono::milliseconds(10)); return isRun = true; }));

	r.wait();

	EXPECT_TRUE(isRun);
}

TEST(work_stealing_thread_pool_test, suspend)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	work_stealing_thread_pool<> pool;

	auto r = boost::when_all(pool.send([&]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, pool.send([]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, pool.send([]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, pool.send([]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, pool.send([]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		);

	r.wait();

	pool.suspend_polling();
	pool.resume_polling();

	std::atomic<bool> isRun(false);
	auto r2 = boost::when_all(pool.send([&]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, pool.send([]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, pool.send([]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, pool.send([]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, pool.send([]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, pool.send([&]() -> bool { std::this_thread::sleep_for(std::chrono::milliseconds(10)); return isRun = true; }));

	r2.wait();

	EXPECT_TRUE(isRun);
}

TEST(work_stealing_thread_pool_test, suspend_exception)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	work_stealing_thread_pool<> pool;

	auto r = boost::when_all(pool.send([&]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, pool.send([]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, pool.send([]() -> void { throw std::exception(); })
		, pool.send([]() -> void { throw std::exception(); })
		, pool.send([]() -> void { throw std::exception(); })
		, pool.send([]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		);

	r.wait();

	pool.suspend_polling();
	pool.resume_polling();

	std::atomic<bool> isRun(false);
	auto r2 = boost::when_all(pool.send([]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, pool.send([]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, pool.send([]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, pool.send([]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
		, pool.send([&]() -> bool { std::this_thread::sleep_for(std::chrono::milliseconds(10)); return isRun = true; }));

	r2.wait();

	EXPECT_TRUE(isRun);
}

TEST(fiber_pool_test, construct)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	
	auto alloc = boost::fibers::fixedsize_stack{64 * 1024};
	{
		fiber_pool<> obj{ 10, alloc };

		std::atomic<bool> isRun{ false };

		std::vector<boost::fibers::future<void>> r;
		r.emplace_back(obj.send([]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(10)); }));
		r.emplace_back(obj.send([]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(10)); }));
		r.emplace_back(obj.send([]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(10)); }));
		r.emplace_back(obj.send([]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(10)); }));
		r.emplace_back(obj.send([]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(10)); }));
		r.emplace_back(obj.send([&]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(10)); isRun = true; }));

		boost::for_each(r, [](const boost::fibers::future<void>& f) { f.wait(); });

		EXPECT_TRUE(isRun);
	}
}

#include <stk/thread/work_stealing_fiber_pool.hpp>
TEST(work_stealing_fiber_pool_test, suspend_exception)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	auto alloc = boost::fibers::fixedsize_stack{64 * 1024};
	work_stealing_fiber_pool<> pool{ 10, alloc };

	std::vector<boost::fibers::future<void>> r;
	r.emplace_back(pool.send([]() -> void { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }));
	r.emplace_back(pool.send([]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(1)); }));
	r.emplace_back(pool.send([]() -> void { throw std::exception(); }));
	r.emplace_back(pool.send([]() -> void { throw std::exception(); }));
	r.emplace_back(pool.send([]() -> void { throw std::exception(); }));
	r.emplace_back(pool.send([]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(1)); }));
	
	boost::for_each(r, [](const boost::fibers::future<void>& f) { f.wait(); });
	r.clear();

	pool.suspend_polling();
	pool.resume_polling();

	std::atomic<bool> isRun(false);
	r.emplace_back(pool.send([]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(1)); }));
	r.emplace_back(pool.send([]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(1)); }));
	r.emplace_back(pool.send([]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(1)); }));
	r.emplace_back(pool.send([]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(1)); }));
	r.emplace_back(pool.send([]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(1)); }));
	r.emplace_back(pool.send([&]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(1)); isRun = true; }));

	boost::for_each(r, [](const boost::fibers::future<void>& f) { f.wait(); });

	EXPECT_TRUE(isRun);
}

#include <stk/thread/thread_specific.hpp>
STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(int);
STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(std::unique_ptr<int>);
TEST(thread_specific_tests, thread_specific_int)
{
	using namespace stk;
	using namespace stk::thread;
	thread_specific<int> sut{ []() { return std::make_shared<int>(10); } };

	std::vector<std::thread> thds;
	for(int i = 0; i < 10; ++i)
	{
		thds.emplace_back([i, &sut]() 
		{
			*sut = i; 
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			int v = *sut;
			EXPECT_EQ(i, v);
		});
	}

	boost::for_each(thds, [](auto& thd) { thd.join(); });
}

TEST(thread_specific_tests, const_thread_specific_int)
{
	using namespace stk;
	using namespace stk::thread;
	const thread_specific<int> sut{ []() { return std::make_shared<int>(10); } };

	std::vector<std::thread> thds;
	for (int i = 0; i < 10; ++i)
	{
		thds.emplace_back([i, &sut]()
		{
			int v = *sut;
			EXPECT_EQ(10, v);
		});
	}

	boost::for_each(thds, [](auto& thd) { thd.join(); });
}

TEST(thread_specific_tests, thread_specific_unique_ptr)
{
	using namespace stk;
	using namespace stk::thread;
	thread_specific<std::unique_ptr<int>> sut{ []() { return std::make_shared<std::unique_ptr<int>>(std::make_unique<int>(10)); } };

	std::vector<std::thread> thds;
	for (int i = 0; i < 10; ++i)
	{
		thds.emplace_back([i, &sut]()
		{
			std::unique_ptr<int>& p = *sut;
			*p = i;
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			std::unique_ptr<int>& p2 = *sut;
			EXPECT_EQ(p.get(), p2.get());
			EXPECT_EQ(i, *p2);
		});
	}

	boost::for_each(thds, [](auto& thd) { thd.join(); });
}

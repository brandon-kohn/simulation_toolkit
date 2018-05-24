#pragma once

#include <gtest/gtest.h>

#include <stk/container/concurrent_integral_map.hpp>
#include <geometrix/numeric/number_comparison_policy.hpp>
#include <stk/thread/work_stealing_thread_pool.hpp>
#include <stk/thread/tiny_atomic_spin_lock.hpp>
#include <stk/thread/concurrentqueue.h>
#ifndef BOOST_NO_CXX11_THREAD_LOCAL
#include <stk/thread/concurrentqueue_queue_info.h>
#include <stk/thread/concurrentqueue_queue_info_no_tokens.h>
using mc_queue_traits = moodycamel_concurrent_queue_traits;
STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(std::uint32_t);
#else
#include <stk/thread/concurrentqueue_queue_info_no_tokens.h>
using mc_queue_traits = moodycamel_concurrent_queue_traits_no_tokens;
#endif

struct null_mutex
{
	null_mutex() = default;
	void lock() {}
	bool try_lock() { return true; }
	void unlock() {}
};

struct cell
{
	static std::int64_t update(int i)
	{
		static std::atomic<std::int64_t> c{ 0 };
		return c.fetch_add(i, std::memory_order_relaxed);
	}

	cell(int id = -1)
		: id(id)
	{
		update(1);
	}

	~cell()
	{
		update(-1);
	}

	int id{ -1 };
};

int extent = 20000;
TEST(concurrent_integral_map_test_suite, construct_and_delete)
{
	using namespace stk;
	using namespace stk::thread;
	{
		concurrent_integral_map<cell> sut;

		cell* pData;
		bool added;
		for (int i = 0; i < extent; ++i)
		{
			auto pNew = new cell(i);
			std::tie(pData, added) = sut.insert(i, pNew);
			EXPECT_EQ(pNew, pData);
			EXPECT_TRUE(added);
		}
		EXPECT_EQ(extent, cell::update(0));
	}

	EXPECT_EQ(0, cell::update(0));
}

TEST(concurrent_integral_map_test_suite, construct_and_erase)
{
	using namespace stk;
	using namespace stk::thread;
	{
		concurrent_integral_map<cell> sut;

		for (int i = 0; i < extent; ++i)
		{
			sut.insert(i, new cell(i));
		}
		EXPECT_EQ(extent, cell::update(0));
		for (int i = 0; i < extent; ++i)
		{
			sut.erase(i);
		}
		EXPECT_EQ(0, cell::update(0));
	}
}

TEST(concurrent_integral_map_test_suite, find)
{
	using namespace stk;
	using namespace stk::thread;
	{
		concurrent_integral_map<cell> sut;

		for (int i = 0; i < extent; ++i)
		{
			sut.insert(i, new cell(i));
		}

		for (int i = 0; i < extent; ++i)
		{
			auto pData = sut.find(i);
			ASSERT_NE(nullptr, pData);
			EXPECT_EQ(i, pData->id);
		}
	}

	EXPECT_EQ(0, cell::update(0));
}

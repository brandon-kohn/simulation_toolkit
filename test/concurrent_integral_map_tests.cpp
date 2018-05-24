#include <gtest/gtest.h>

#include <stk/container/concurrent_integral_map.hpp>
#include <geometrix/numeric/number_comparison_policy.hpp>
#include <stk/thread/work_stealing_thread_pool.hpp>
#include <stk/thread/tiny_atomic_spin_lock.hpp>
#include <stk/thread/concurrentqueue.h>
#include <stk/thread/concurrentqueue_queue_info_no_tokens.h>
#include <boost/smart_ptr/make_unique.hpp>

using mc_queue_traits = moodycamel_concurrent_queue_traits_no_tokens;

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
TEST(concurrent_integral_map_test_suite, contruct_insert_destruct)
{
	using namespace stk;
	using namespace stk::thread;
	{
		concurrent_integral_map<cell> sut;

		cell* pData;
		bool added;
		for (int i = 0; i < extent; ++i)
		{
			std::tie(pData, added) = sut.insert(i, boost::make_unique<cell>(i));
			EXPECT_TRUE(added);
		}
		junction::DefaultQSBR().flush();
		EXPECT_EQ(extent, cell::update(0));
	}

	EXPECT_EQ(0, cell::update(0));
}

TEST(concurrent_integral_map_test_suite, insert_and_erase)
{
	using namespace stk;
	using namespace stk::thread;
	{
		concurrent_integral_map<cell> sut;

		for (int i = 0; i < extent; ++i)
		{
			sut.insert(i, boost::make_unique<cell>(i));
		}
		EXPECT_EQ(extent, cell::update(0));
		for (int i = 0; i < extent; ++i)
		{
			sut.erase(i);
		}
		junction::DefaultQSBR().flush();
		EXPECT_EQ(0, cell::update(0));
	}
}

TEST(concurrent_integral_map_test_suite, emplace_and_erase)
{
	using namespace stk;
	using namespace stk::thread;
	{
		concurrent_integral_map<cell> sut;

		for (int i = 0; i < extent; ++i)
		{
			sut.emplace(i, i);
		}
		EXPECT_EQ(extent, cell::update(0));
		for (int i = 0; i < extent; ++i)
		{
			sut.erase(i);
		}
		junction::DefaultQSBR().flush();
		EXPECT_EQ(0, cell::update(0));
	}
}

TEST(concurrent_integral_map_test_suite, find_inserted)
{
	using namespace stk;
	using namespace stk::thread;
	{
		concurrent_integral_map<cell> sut;

		for (int i = 0; i < extent; ++i)
		{
			sut.insert(i, boost::make_unique<cell>(i));
		}

		for (int i = 0; i < extent; ++i)
		{
			auto pData = sut.find(i);
			ASSERT_NE(nullptr, pData);
			EXPECT_EQ(i, pData->id);
		}
	}
	
	junction::DefaultQSBR().flush();
	EXPECT_EQ(0, cell::update(0));
}

TEST(concurrent_integral_map_test_suite, find_emplaced)
{
	using namespace stk;
	using namespace stk::thread;
	{
		concurrent_integral_map<cell> sut;

		for (int i = 0; i < extent; ++i)
		{
			sut.emplace(i, i);
		}

		for (int i = 0; i < extent; ++i)
		{
			auto pData = sut.find(i);
			ASSERT_NE(nullptr, pData);
			EXPECT_EQ(i, pData->id);
		}
	}
	
	junction::DefaultQSBR().flush();
	EXPECT_EQ(0, cell::update(0));
}

TEST(concurrent_integral_map_test_suite, insert_existing)
{
	using namespace stk;
	using namespace stk::thread;
	{
		concurrent_integral_map<cell> sut;

		for (int i = 0; i < extent; ++i)
		{
			sut.insert(i, boost::make_unique<cell>(i));
		}
		EXPECT_EQ(extent, cell::update(0));

		for (int i = 0; i < extent; ++i)
		{
			sut.insert(i, boost::make_unique<cell>(i));
		}
		EXPECT_EQ(extent, cell::update(0));
	}

	junction::DefaultQSBR().flush();
	EXPECT_EQ(0, cell::update(0));
}

TEST(concurrent_integral_map_test_suite, emplace_existing)
{
	using namespace stk;
	using namespace stk::thread;
	{
		concurrent_integral_map<cell> sut;

		for (int i = 0; i < extent; ++i)
		{
			sut.insert(i, boost::make_unique<cell>(i));
		}
		EXPECT_EQ(extent, cell::update(0));

		for (int i = 0; i < extent; ++i)
		{
			auto r = sut.emplace(i, i);
			EXPECT_FALSE(r.second);
		}
		EXPECT_EQ(extent, cell::update(0));
	}

	junction::DefaultQSBR().flush();
	EXPECT_EQ(0, cell::update(0));
}

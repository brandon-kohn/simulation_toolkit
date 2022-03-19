#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <stk/container/concurrent_pointer_unordered_map.hpp>
#include <stk/container/concurrent_numeric_unordered_map.hpp>
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
TEST(concurrent_pointer_unordered_map_test_suite, contruct_insert_destruct)
{
	using namespace stk;
	using namespace stk::thread;
	{
		concurrent_pointer_unordered_map<std::uint64_t, cell> sut;

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

TEST(concurrent_pointer_unordered_map_test_suite, insert_and_erase)
{
	using namespace stk;
	using namespace stk::thread;
	{
		concurrent_pointer_unordered_map<std::uint64_t, cell> sut;

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

TEST(concurrent_pointer_unordered_map_test_suite, find_inserted)
{
	using namespace stk;
	using namespace stk::thread;
	{
		concurrent_pointer_unordered_map<std::uint64_t, cell> sut;

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

TEST(concurrent_pointer_unordered_map_test_suite, insert_existing)
{
	using namespace stk;
	using namespace stk::thread;
	{
		concurrent_pointer_unordered_map<std::uint64_t, cell> sut;

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

TEST(concurrent_pointer_unordered_map_test_suite, iterators_trivial)
{
	using namespace stk;
	using namespace stk::thread;
	concurrent_pointer_unordered_map<std::uint64_t, cell> sut;

	EXPECT_EQ(sut.begin(), sut.end());
	EXPECT_EQ(sut.cbegin(), sut.cend());
}

TEST(concurrent_pointer_unordered_map_test_suite, iterator_compare)
{
	using namespace stk;
	using namespace stk::thread;
	{
		concurrent_pointer_unordered_map<std::uint64_t, cell> sut;

		for (int i = 0; i < extent; ++i)
		{
			sut.insert(i, boost::make_unique<cell>(i));
		}

		for (auto it1 = sut.begin(), it2 = sut.begin(); it1 != sut.cend(); ++it1, ++it2)
		{
			EXPECT_EQ(it1, it2);
			it1->id *= 10;
		}
		
		for (auto it1 = sut.cbegin(), it2 = sut.cbegin(); it1 != sut.cend(); ++it1, ++it2)
		{
			EXPECT_EQ(it1, it2);
		}
	}

	junction::DefaultQSBR().flush();
	EXPECT_EQ(0, cell::update(0));
}

TEST(concurrent_numeric_unordered_map_test_suite, insert)
{
	using namespace stk;
	using ::testing::NotNull;
	auto sut = concurrent_numeric_unordered_map<std::uint64_t, int>{};

	sut.insert(10, 20);
	ASSERT_TRUE(sut.find(10));
	EXPECT_EQ(20, *sut.find(10));
}

TEST(concurrent_numeric_unordered_map_test_suite, erase)
{
	using namespace stk;
	using ::testing::NotNull;
	auto sut = concurrent_numeric_unordered_map<std::uint64_t, int>{};

	sut.insert(10, 20);
	sut.erase(10);
	EXPECT_FALSE(sut.find(10));
}

TEST(concurrent_numeric_unordered_map_test_suite, pointer_key_insert)
{
	using namespace stk;
	using ::testing::NotNull;
	auto sut = concurrent_numeric_unordered_map<int*, int*>{};

	int* key0 = reinterpret_cast<int*>(static_cast<std::size_t>(0xBAADF00D));
	int* data = reinterpret_cast<int*>(static_cast<std::size_t>(0xBAADF00D));
	sut.insert(key0, data);
	sut.erase(key0);
	EXPECT_FALSE(sut.find(key0));
}

TEST(concurrent_numeric_unordered_map_test_suite, iterator_compare)
{
	using namespace stk;
	using namespace stk::thread;
	{
		using sut_t = concurrent_numeric_unordered_map<int*, int*>;
		auto sut = sut_t{};

		for (std::ptrdiff_t i = 3; i < extent; ++i) 
		{
			sut.insert((int*)i, (int*)i);
		}

		for (auto it1 = sut.cbegin(), it2 = sut.cbegin(); it1 != sut.cend(); ++it1, ++it2) 
		{
			EXPECT_EQ(it1, it2);
		}
	}
}

TEST(concurrent_pointer_unordered_map_test_suite, death_on_null_insert)
{
	using namespace stk;
	auto sut = concurrent_pointer_unordered_map<int*, int>{};
	ASSERT_DEBUG_DEATH(sut.insert(0, std::unique_ptr<int>{}), "\\.+");
}

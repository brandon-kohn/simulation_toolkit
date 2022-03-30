#include <stk/container/small_flat_set.hpp>
#include <stk/container/small_flat_map.hpp>
#include <ska/bytell_hash_map.hpp>
#include <geometrix/utility/scope_timer.ipp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

TEST(small_flat_set_test_suite, construct_small_flat_set)
{
	using namespace stk;
	auto sut = stk::small_flat_set<int, 10>{};

	EXPECT_EQ( 0u, sut.size() );
}

TEST(small_flat_set_test_suite, construct_small_flat_set_fewer_than_N)
{
	using namespace stk;

	stk::small_flat_set<int, 10> sut = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };

	EXPECT_EQ( 9u, sut.size() );
}

TEST(small_flat_set_test_suite, construct_small_flat_set_more_than_N)
{
	using namespace stk;

	stk::small_flat_set<int, 10> sut = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

	EXPECT_EQ( 11u, sut.size() );
}

TEST(small_flat_map_test_suite, construct_small_flat_map)
{
	using namespace stk;
	auto sut = stk::small_flat_map<int, int, 10>{};

	EXPECT_EQ( 0u, sut.size() );
}

TEST(small_flat_map_test_suite, construct_small_flat_map_fewer_than_N)
{
	using namespace stk;

	stk::small_flat_map<int, int, 10> sut = { { 0, 0 },
		{ 1, 1 },
		{ 2, 2 },
		{ 3, 3 },
		{ 4, 4 },
		{ 5, 5 },
		{ 6, 6 },
		{ 7, 7 },
		{ 8, 8 }
	};

	EXPECT_EQ( 9u, sut.size() );
}

TEST(small_flat_map_test_suite, construct_small_flat_map_more_than_N)
{
	using namespace stk;

	stk::small_flat_map<int, int, 10> sut = { { 0, 0 },
		{ 1, 1 },
		{ 2, 2 },
		{ 3, 3 },
		{ 4, 4 },
		{ 5, 5 },
		{ 6, 6 },
		{ 7, 7 },
		{ 8, 8 },
		{ 9, 9 },
		{ 10, 10 }
	};

	EXPECT_EQ( 11u, sut.size() );
}

#include <random>
auto nRuns = 10000000;
struct random_seed_seq
{
	template <typename It>
	void generate( It begin, It end )
	{
		for( ; begin != end; ++begin )
		{
			*begin = device();
		}
	}

	static random_seed_seq& get_instance()
	{
		static thread_local random_seed_seq result;
		return result;
	}

private:
	std::random_device device;
};
TEST_P(flat_map_test_suite, timings)
{
	std::vector<void*> data(GetParam());
	std::vector<void*> results( GetParam() );
	std::mt19937_64    random_source{ 0xBAADF00DDEADBEEF };

	for (auto i = 0ULL; i < GetParam(); ++i)
	{
		data[i] = (void*)random_source();
	}

	std::stringstream name;
	boost::container::flat_map<void*, void*> sut;

	name << "flat_map_void*_inserts_in_table_";
	name << std::setfill('0') << std::setw(2) << GetParam();
	{
		GEOMETRIX_MEASURE_SCOPE_TIME(name.str().c_str());
		for( auto i = 0ULL; i < nRuns; ++i )
		{
			sut.insert_or_assign( data[i], data[i] );
		}
	}

	name.clear();
	name << "flat_map_void*_lookups_in_table_";
	name << std::setfill('0') << std::setw(2) << GetParam();
	{
		GEOMETRIX_MEASURE_SCOPE_TIME(name.str().c_str());
		for( auto i = 0ULL; i < nRuns; ++i )
		{
			results[i] = *sut.find( data[i]);
		}
	}
}

TEST_P(bytell_hash_map_test_suite, timings)
{
	std::vector<void*> data(GetParam());
	std::vector<void*> results( GetParam() );
	std::mt19937_64    random_source{ 0xBAADF00DDEADBEEF };

	for (auto i = 0ULL; i < GetParam(); ++i)
	{
		data[i] = (void*)random_source();
	}

	std::stringstream name;
	ska::bytell_hash_map<void*, void*> sut;

	name << "bytell_hash_map_default_hasher_void*_inserts_in_table_";
	name << std::setfill('0') << std::setw(2) << GetParam();
	{
		GEOMETRIX_MEASURE_SCOPE_TIME(name.str().c_str());
		for( auto i = 0ULL; i < nRuns; ++i )
		{
			sut.insert_or_assign( data[i], data[i] );
		}
	}

	name.clear();
	name << "bytell_hash_map_default_hasher_void*_lookups_in_table_";
	name << std::setfill('0') << std::setw(2) << GetParam();
	{
		GEOMETRIX_MEASURE_SCOPE_TIME(name.str().c_str());
		for( auto i = 0ULL; i < nRuns; ++i )
		{
			results[i] = *sut.find( data[i]);
		}
	}
}
INSTANTIATE_TEST_CASE_P(timings, bytell_hash_map_test_suite, ::testing::Range(10, 1000));
INSTANTIATE_TEST_CASE_P(timings, flat_map_test_suite, ::testing::Range(10, 1000));

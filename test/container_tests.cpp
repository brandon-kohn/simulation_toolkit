#if BOOST_VERSION >= 107000
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
auto nRuns = 10000;
class flat_map_test_suite : public ::testing::TestWithParam<int>
{
public:
	flat_map_test_suite()
	{}

protected:
	virtual void SetUp()
	{
		using namespace ::testing;
	}
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

	boost::container::flat_map<void*, void*> sut;

	{
		std::stringstream name;
		name << "flat_map_void*_inserts_in_table_";
		name << std::setfill( '0' ) << std::setw( 4 ) << GetParam();
		{
			GEOMETRIX_MEASURE_SCOPE_TIME( name.str().c_str() );
			for( auto i = 0ULL; i < GetParam(); ++i )
			{
				sut.insert_or_assign( data[i], data[i] );
			}
		}
	}

	{
		std::stringstream name;
		name << "flat_map_void*_lookups_in_table_";
		name << std::setfill( '0' ) << std::setw( 4 ) << GetParam();
		{
			GEOMETRIX_MEASURE_SCOPE_TIME( name.str().c_str() );
			for( auto i = 0ULL; i < nRuns; ++i )
			{
				for( auto j = 0ULL; j < GetParam(); ++j )
					results[j] = sut.find( data[j] )->second;
			}
		}
	}
}
class bytell_hash_map_test_suite : public ::testing::TestWithParam<int>
{
public:
	bytell_hash_map_test_suite()
	{}

protected:
	virtual void SetUp()
	{
		using namespace ::testing;
	}
};

TEST_P(bytell_hash_map_test_suite, timings)
{
	std::vector<void*> data(GetParam());
	std::vector<void*> results( GetParam() );
	std::mt19937_64    random_source{ 0xBAADF00DDEADBEEF };

	for (auto i = 0ULL; i < GetParam(); ++i)
	{
		data[i] = (void*)random_source();
	}

	ska::bytell_hash_map<void*, void*> sut;

	{
		std::stringstream name;
		name << "bytell_hash_map_default_hasher_void*_inserts_in_table_";
		name << std::setfill( '0' ) << std::setw( 4 ) << GetParam();
		{
			GEOMETRIX_MEASURE_SCOPE_TIME( name.str().c_str() );
			for( auto i = 0ULL; i < GetParam(); ++i )
			{
				sut.insert_or_assign( data[i], data[i] );
			}
		}
	}

	{
		std::stringstream name;
		name << "bytell_hash_map_default_hasher_void*_lookups_in_table_";
		name << std::setfill( '0' ) << std::setw( 4 ) << GetParam();
		{
			GEOMETRIX_MEASURE_SCOPE_TIME( name.str().c_str() );
			for( auto i = 0ULL; i < nRuns; ++i )
			{
				for( auto j = 0ULL; j < GetParam(); ++j )
					results[j] = sut.find( data[j] )->second;
			}
		}
	}
}
INSTANTIATE_TEST_CASE_P(timings, bytell_hash_map_test_suite, ::testing::Range(1, 200, 5));
INSTANTIATE_TEST_CASE_P(timings, flat_map_test_suite, ::testing::Range(1, 200, 5));
#endif

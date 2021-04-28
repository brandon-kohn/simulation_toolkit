#include <stk/container/small_flat_set.hpp>
#include <stk/container/small_flat_map.hpp>

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
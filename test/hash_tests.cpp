#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <stk/utility/string_hash.hpp>

TEST( string_hash_test_suite, construct_hash )
{
	using namespace stk;

	constexpr auto h = "Here is a human readable string to hash."_hash;

	static_assert( h == 2982158385307036437, "Should be correct at compile time." );
	EXPECT_EQ( h, 2982158385307036437u );
}

TEST( string_hash_test_suite, construct_hash64 )
{
	using namespace stk;

	constexpr auto h = "Here is a human readable string to hash."_hash64;

	static_assert( h == 2982158385307036437u, "Should be correct at compile time." );
	EXPECT_EQ( h, 2982158385307036437u );
}

TEST( string_hash_test_suite, construct_hash32 )
{
	using namespace stk;

	constexpr auto h = "Here is a human readable string to hash."_hash32;

	static_assert( h == 1377865077u, "Should be correct at compile time." );
	EXPECT_EQ( h, 1377865077u );
}

TEST( string_hash_test_suite, construct_hash_repeatable )
{
	using namespace stk;

	constexpr auto h0 = "Here is a human readable string to hash."_hash;
	constexpr auto h1 = "Here is a human readable string to hash."_hash;

	static_assert( h1 == h0, "Should be correct at compile time." );
	EXPECT_EQ( h1, h0 );
}

TEST( string_hash_test_suite, construct_string_hash )
{
	using namespace stk;

	constexpr auto key = "HiHiHi!";
	constexpr auto s32_ = make_string_hash<std::uint32_t>( key );
	constexpr auto s64_ = make_string_hash<std::uint64_t>( key );
	constexpr auto s32 = make_string_hash32( key );
	constexpr auto s64 = make_string_hash64( key );

	static_assert( s32.key() == key, "Should be correct at compile time." );
	static_assert( s32.hash() == 2833342361u, "Should be correct at compile time." );
	static_assert( s64.key() == key, "Should be correct at compile time." );
	static_assert( s64.hash() == 9716672729628056121u, "Should be correct at compile time." );
	static_assert( s64 == s64_, "Should be correct at compile time." );
	static_assert( s32 == s32_, "Should be correct at compile time." );
	EXPECT_EQ( s32.hash(), 2833342361u );
	EXPECT_EQ( s64.hash(), 9716672729628056121u );
}

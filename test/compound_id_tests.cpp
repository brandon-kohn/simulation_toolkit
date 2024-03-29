
#include <stk/utility/compound_id.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

TEST( compound_id_test_suite, construct_default )
{
	using namespace stk;

	struct id_state
	{
		std::uint32_t hi;
		std::uint32_t lo;
	};
	union
	{
		id_state      id;
		std::uint64_t v;
	};
	id.lo = 0;
	id.hi = std::numeric_limits<std::uint32_t>::max();
	auto id_ = compound_id_impl<std::uint64_t, 32>{};

	EXPECT_EQ( 0, id_.value() );
}

TEST( compound_id_test_suite, construct_hi )
{
	using namespace stk;

	struct id_state
	{
		std::uint32_t hi;
		std::uint32_t lo;
	};
	union
	{
		id_state      id;
		std::uint64_t v;
	};
	id.lo = 0;
	id.hi = std::numeric_limits<std::uint32_t>::max();
	auto id_ = compound_id<32>{ id.hi };

	EXPECT_EQ( id.hi, get<0>(id_) );
}

TEST(compound_id_test_suite, construct_hi_lo )
{
	using namespace stk;

	struct id_state
	{
		std::uint32_t hi;
		std::uint32_t lo;
	};
	union
	{
		id_state      id;
		std::uint64_t v;
	};
	id.lo = 0;
	id.hi = std::numeric_limits<std::uint32_t>::max();
	auto id_ = compound_id<32>{ id.hi, id.lo };

	auto r = get<0>( id_ );
	EXPECT_EQ( id.hi, r );
	r = get<1>( id_ );
	EXPECT_EQ( id.lo, r );

	set<0>( 0, id_ );
	r = get<0>( id_ );
	EXPECT_EQ( 0, r );
	r = get<1>( id_ );
	EXPECT_EQ( id.lo, r );
	set<1>( 0, id_ );
	r = get<1>( id_ );
	EXPECT_EQ( 0, r );
}

TEST(compound_id_test_suite, construct_three_components)
{
	using namespace stk;

	struct id_state
	{
		std::uint32_t hi;
		std::uint16_t mid;
		std::uint16_t lo;
	};
	union
	{
		id_state      id;
		std::uint64_t v;
	};
	id.lo = 22;
	id.mid = 69; 
	id.hi = std::numeric_limits<std::uint32_t>::max();
	auto id_ = compound_id<32, 48>{ id.hi, id.mid, id.lo };

	auto r = get<0>( id_ );
	EXPECT_EQ( id.hi, r );
	r = get<1>( id_ );
	EXPECT_EQ( id.mid, r );
	r = get<2>( id_ );
	EXPECT_EQ( id.lo, r );
	
	set<0>( 33, id_ );
	r = get<0>( id_ );
	EXPECT_EQ( 33, r );
	r = get<1>( id_ );
	EXPECT_EQ( id.mid, r );
	r = get<2>( id_ );
	EXPECT_EQ( id.lo, r );

	set<1>( 70, id_ );
	r = get<1>( id_ );
	EXPECT_EQ( 70, r );
	r = get<0>( id_ );
	EXPECT_EQ( 33, r );
	r = get<2>( id_ );
	EXPECT_EQ( id.lo, r );
	
	set<2>( 99, id_ );
	r = get<2>( id_ );
	EXPECT_EQ( 99, r );
	r = get<0>( id_ );
	EXPECT_EQ( 33, r );
	r = get<1>( id_ );
	EXPECT_EQ( 70, r );
}

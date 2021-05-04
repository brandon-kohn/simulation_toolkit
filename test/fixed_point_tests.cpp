
#include <geometrix/numeric/fixed_point.hpp>
#include <geometrix/numeric/boost_units_quantity_fixed_point.hpp>
#include <gtest/gtest.h>
#include <cstdint>

using fixed_point_3_traits = geometrix::fixed_point_traits
<
	std::int64_t
  , geometrix::generic_compile_time_scale_policy<10, 3>
  , geometrix::round_half_to_even
>;

using fixed_point3 = geometrix::fixed_point<fixed_point_3_traits>;

TEST(fixed_point_suite, construct_from_double)
{
	fixed_point3 sut(123.4567);

	EXPECT_EQ(123.457, sut);
}

TEST(fixed_point_suite, addition)
{
	fixed_point3 sut(123.4567);

	EXPECT_EQ(133.457, sut + 10.0);
	EXPECT_EQ(133.457, 10. + sut);
}

TEST(fixed_point_suite, subtraction)
{
	fixed_point3 sut(123.4567);

	EXPECT_EQ(113.457, sut - 10.0);
	EXPECT_EQ(-113.457, 10. - sut);
}


TEST(fixed_point_suite, multiplication)
{
	fixed_point3 sut(123.4567);

	EXPECT_EQ(1234.57, sut * 10.0);
	EXPECT_EQ(1234.57, 10. * sut);
}

TEST(fixed_point_suite, division)
{
	fixed_point3 sut(123.4567);

	EXPECT_EQ(12.345, sut / 10.0);
}

TEST(fixed_point_suite, division1_3)
{
	fixed_point3 sut(1.0);

	EXPECT_EQ(0.333, sut / 3.0);
}

TEST(fixed_point_suite, division_fixed_1_3)
{
	fixed_point3 sut1(1.0);
	fixed_point3 sut3(3.0);

	EXPECT_EQ(0.333, sut1 / sut3);
}

TEST(fixed_point_suite, division_fixed_1_3_rvalue)
{
	EXPECT_EQ(0.333, fixed_point3(1.0) / fixed_point3(3.0));
}

#include <boost/icl/interval_set.hpp>
#include <geometrix/numeric/constants.hpp>
#include <geometrix/numeric/boost_units_quantity.hpp>
#include <boost/units/cmath.hpp>
#include <geometrix/algebra/expression.hpp>
#include <stk/units/boost_units.hpp>
#include <stk/geometry/tolerance_policy.hpp>

//! Traits to give 1 whole number and 18 decimal places in fixed precision for representing normalized angles in the range of either [-2pi, 0) or [0, 2pi].
using fixed_point_radian_traits = geometrix::fixed_point_traits
<
    std::int64_t
  , geometrix::generic_compile_time_scale_policy<10, 5>
  , geometrix::round_half_to_even
>;

using radian_base = geometrix::fixed_point<fixed_point_radian_traits>;
using radian = boost::units::quantity<boost::units::si::plane_angle, radian_base>;

namespace geometrix {

}//! namespace geometrix;

TEST(fixed_point_suite, radian_equal_pi)
{
	//! This tests for change of the construction of constants from the double representation.
	auto pi = geometrix::constants::pi<radian>();
	EXPECT_EQ(pi, geometrix::constants::pi<double>() * boost::units::si::radians);
}

TEST(fixed_point_suite, radian_fmod)
{
	using std::fmod;
	auto pi = geometrix::constants::pi<radian>();
	//! This tests for change of the construction of constants from the double representation.
	auto threepi = 3.0 * pi;
	auto r = fmod(threepi, geometrix::constants::two_pi<radian>());
	//! Expect the values to differ in the least significant digit due to rounding by 1. Hence 5 decimal places tolerance on the compare.
	EXPECT_TRUE(stk::make_tolerance_policy(1e-5).equals(r, geometrix::constants::pi<radian>()));
}

#include <geometrix/utility/vector_angle_compare.hpp>
#include <geometrix/primitive/point.hpp>
#include <geometrix/tensor/vector.hpp>
#include <geometrix/tensor/is_null.hpp>

TEST(interval_set_test_suit, test2)
{
	using namespace geometrix;
	using vector2 = vector_double_2d;
	using point2 = point_double_2d;
	std::vector<point2> points;
	std::vector<vector2> vectors;
	std::vector<double> pangles;

	auto p = point2{ 0., 0. };
	auto v = vector2{ 1.0, 0.0 };
	auto s = 2.0 * constants::pi<double>() / 100.0;
	for (auto i = 0UL; i < 101; ++i)
	{
		points.emplace_back(p + v);
		vectors.emplace_back(v);
		pangles.emplace_back(pseudo_angle(v));
		auto lv = vector2{ s * normalize(perp(v)) };
		v = normalize(v + lv);
	}

	EXPECT_TRUE(true);
}

TEST(interval_set_test_suit, test)
{
	using namespace boost::icl;
	using iset = boost::icl::interval_set<radian>;
	using interval_t = iset::interval_type;

	auto sut = iset{};

	sut.insert(interval_t(0, geometrix::constants::pi<radian>()));
	sut.insert(interval_t(geometrix::constants::pi<radian>() + 0.1 * boost::units::si::radians, geometrix::constants::two_pi<radian>()));

	auto v = geometrix::constants::pi<radian>() + 0.05 * boost::units::si::radians;
	auto key = interval_t::closed(v, v);
	auto it = sut.lower_bound(key);

	EXPECT_FALSE(contains(*it, key));

	std::cout << sut << std::endl;
	std::cout << *it << std::endl;
}

#define STK_DONT_USE_PREPROCESSED_FILES
#include <stk/utility/unsigned_integer.hpp>
using namespace stk;
TEST(unsigned_integer_test_suite, cast_test)
{
	using namespace ::testing;
	using namespace stk::detail;

	static_assert(std::is_same<safe_compare_cast_policy_selector<unsigned int, int>::type, convert_to_unsigned_U>::value, "conversion should up-convert U to unsigned.");
	static_assert(std::is_same<safe_compare_cast_policy_selector<unsigned int, unsigned char>::type, use_ansi_conversion>::value, "conversion should use ansi conversion.");
  
	{
		auto a = safe_compare_cast<unsigned int, int>(static_cast<unsigned int>(20));
		auto b = safe_compare_cast<unsigned int, int>(static_cast<int>(10));
		static_assert(std::is_same<decltype(a), unsigned int>::value, "should be unsigned int.");
		static_assert(std::is_same<decltype(b), unsigned int>::value, "should be unsigned int.");
	}

	{
		auto a = safe_compare_cast<int, unsigned int>(static_cast<unsigned int>(20));
		auto b = safe_compare_cast<int, unsigned int>(static_cast<int>(10));
		static_assert(std::is_same<decltype(a), unsigned int>::value, "should be unsigned int.");
		static_assert(std::is_same<decltype(b), unsigned int>::value, "should be unsigned int.");
	}

	{
		auto a = safe_compare_cast<int, double>(static_cast<double>(20.));
		auto b = safe_compare_cast<int, double>(static_cast<int>(10));
		static_assert(std::is_same<decltype(a), double>::value, "should be double.");
		static_assert(std::is_same<decltype(b), int>::value, "should be int.");
	}

	{
		auto a = safe_compare_cast<int, int>(int(20));
		auto b = safe_compare_cast<int, int>(int(10));
		static_assert(std::is_same<decltype(a), int>::value, "should be int.");
		static_assert(std::is_same<decltype(b), int>::value, "should be int.");
	}

	{
		auto a = safe_compare_cast<char, char>(char(20));
		auto b = safe_compare_cast<char, char>(char(10));
		static_assert(std::is_same<decltype(a), char>::value, "should be char.");
		static_assert(std::is_same<decltype(b), char>::value, "should be char.");
	}
}

template <typename T>
class unsigned_integer_typed_test_fixture : public ::testing::Test
{
public:

	unsigned_integer_typed_test_fixture()
	{}
};

typedef ::testing::Types
<
    char
  , signed char
  , short
  , int
  , long
  , long long
  , float
  , double
  , long double
> unsigned_integer_typed_test_signed_types;

typedef ::testing::Types
<
    unsigned char
  , unsigned short
  , unsigned int
  , unsigned long
> unsigned_integer_typed_test_unsigned_types;

typedef ::testing::Types
<
    char
  , unsigned char
  , signed char
  , short
  , unsigned short
  , int
  , unsigned int
  , long
  , unsigned long
  , long long
  , unsigned long long
  , float
  , double
  , long double
> unsigned_integer_typed_test_types;

TYPED_TEST_CASE(unsigned_integer_typed_test_fixture, unsigned_integer_typed_test_types);

TYPED_TEST( unsigned_integer_typed_test_fixture, unsigned_integer_comparison_tests )
{
	using namespace ::testing;
	index ten(10);
	index zero(0);
	if (std::is_signed<TypeParam>::value)
	{
		TypeParam minus1 = -1;
		EXPECT_LT(minus1, ten);
		EXPECT_GT(ten, minus1);
		EXPECT_NE(ten, minus1);
		EXPECT_NE(minus1, ten);
	}
	
	if ( std::is_floating_point<TypeParam>::value )
	{
		TypeParam fraction = static_cast<TypeParam>(1) / static_cast<TypeParam>(2);
		EXPECT_LT(zero, fraction);
		EXPECT_GT(fraction, zero);
		EXPECT_NE(fraction, zero);
		EXPECT_NE(zero, fraction);
	}

	TypeParam tfifty = 50;
	EXPECT_LT(ten, tfifty);
	EXPECT_GT(tfifty, ten);
	EXPECT_NE(ten, tfifty);
	EXPECT_NE(tfifty, ten);

	EXPECT_EQ(ten, ten);
	EXPECT_LE(ten, ten);
	EXPECT_GE(ten, ten);

	TypeParam tten = 10;
	EXPECT_EQ(ten, tten);
	EXPECT_LE(ten, tten);
	EXPECT_GE(ten, tten);
	EXPECT_EQ(tten, ten);
	EXPECT_LE(tten, ten);
	EXPECT_GE(tten, ten);

	TypeParam tzero = 0;
	EXPECT_EQ(zero, tzero);
	EXPECT_LE(zero, tzero);
	EXPECT_GE(zero, tzero);
}

TEST(unsigned_integer_test_suite, test_invalid_states)
{
	using namespace ::testing;
	index invalid;
	EXPECT_TRUE(invalid.is_invalid());
	EXPECT_FALSE(invalid.is_valid());

	//! Except can compare invalid with other invalid values.
	EXPECT_EQ(invalid, invalid);
	EXPECT_EQ(invalid, index::invalid);
	EXPECT_EQ(invalid, -1);
	EXPECT_FALSE(invalid != invalid);
}

TEST(unsigned_integer_test_suite, test_overflow)
{
	using namespace ::testing;
	index i((std::numeric_limits<index>::max)());
	EXPECT_TRUE(i.is_valid());
	++i;
	EXPECT_TRUE(i.is_invalid());

	index zero(0);
	EXPECT_TRUE((i * zero).is_invalid());
	EXPECT_TRUE((zero * i).is_invalid());
	EXPECT_TRUE((i + zero).is_invalid());
	EXPECT_TRUE((zero + i).is_invalid());

	EXPECT_TRUE((i * 2).is_invalid());
	EXPECT_TRUE((2 * i).is_invalid());

	EXPECT_TRUE((i + 2).is_invalid());
	EXPECT_TRUE((2 + i).is_invalid());

	index hi = i / 2;
	EXPECT_EQ(i, hi * 2);
}

TEST(unsigned_integer_test_suite, test_underflow)
{
	using namespace ::testing;
	index i(0);
	EXPECT_TRUE(i.is_valid());
	--i;
	EXPECT_TRUE(i.is_invalid());

	EXPECT_TRUE((i * -1).is_invalid());
	EXPECT_TRUE((i - 1).is_invalid());
	EXPECT_TRUE((i + -1).is_invalid());

	EXPECT_TRUE((i * -10).is_invalid());
	EXPECT_TRUE((i - 10).is_invalid());
	EXPECT_TRUE((i + -10).is_invalid());
}

TEST(unsigned_integer_test_suite, test_bool_conversion)
{
	using namespace ::testing;
	index i(0);
	EXPECT_TRUE(i == false);
	EXPECT_TRUE(false == (bool)i);
	EXPECT_FALSE(i);
	EXPECT_TRUE(!i);
	i = 1;
	EXPECT_TRUE(i == true);
	EXPECT_TRUE(true == (bool)i);
	EXPECT_TRUE(i);
	EXPECT_FALSE(!i);

	i = 100;
	EXPECT_TRUE(i == true);
	EXPECT_TRUE(true == (bool)i);
	EXPECT_TRUE(i);
	EXPECT_FALSE(!i);
}

TEST(unsigned_integer_test_suite, test_conversion)
{
	using namespace ::testing;
	unsigned_integer<unsigned int> ui;
	unsigned_integer<unsigned long long> ull;

	ui = ull;
	EXPECT_TRUE(ui.is_invalid());
	
	unsigned_integer<unsigned short> us(ui);
	EXPECT_TRUE(us.is_invalid());
}

TYPED_TEST(unsigned_integer_typed_test_fixture, test_addition)
{
	using namespace ::testing;
	index zero(0);
	index ten(10);
	index five(5);

	TypeParam tten = 10;
	TypeParam tfive = 5;
	EXPECT_EQ(zero + tten, 10);
	EXPECT_EQ(ten + tten, 20);
	EXPECT_EQ(ten - tfive, five);

	if (std::is_signed<TypeParam>::value)
	{
		TypeParam minus1 = -1;
		EXPECT_TRUE((zero + minus1).is_invalid());
	}

	if (std::is_floating_point<TypeParam>::value)
	{
		TypeParam fraction = static_cast<TypeParam>(1) / static_cast<TypeParam>(2);
		EXPECT_EQ(zero + fraction, zero);
	}
}

TEST(unsigned_integer_test_suite, DISABLED_test_assigned_from_pointer)
{
	using namespace ::testing;
	int   junk;
	int*  iptr = &junk;
	//index no = iptr;
}
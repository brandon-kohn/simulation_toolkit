
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
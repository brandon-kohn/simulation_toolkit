#include <geometrix/numeric/fixed_point.hpp>
#include <gtest/gtest.h>
#include <cstdint>

using fixed_point_3_traits = geometrix::fixed_point_traits
<
	geometrix::decimal_format_traits< std::int64_t >,
	geometrix::generic_compile_time_scale_policy<10, 3>,
	geometrix::round_half_to_even
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
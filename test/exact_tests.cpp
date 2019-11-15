
#include <exact/predicates.hpp>
#include <stk/geometry/tolerance_policy.hpp>
#include <stk/geometry/primitive/segment.hpp>
#include <geometrix/algorithm/orientation.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

struct exact_test_suite : ::testing::Test
{
	exact_test_suite()
	{
		exact::init();
	}
};

using mpfloat = boost::multiprecision::cpp_dec_float_100;
using mppoint2 = geometrix::point<mpfloat, 2>;
using mpvector2 = geometrix::vector<mpfloat, 2>;

inline geometrix::orientation_type Orientation(mpfloat x0, mpfloat x1)
{
	using namespace geometrix;
	if (x0 < x1)
		return oriented_right;
	else if (x0 > x1)
		return oriented_left;
	else
		return oriented_collinear;
}

//! Orientation test to check the orientation of B relative to A.
//! @precondition A and B are vectors which share a common origin.
inline geometrix::orientation_type get_Orientation(mpvector2 A, mpvector2 B)
{
	using namespace geometrix;
	return Orientation(get<0>(A) * get<1>(B), get<1>(A) * get<0>(B));
}

inline geometrix::orientation_type get_Orientation( const mppoint2& A, const mppoint2& B, const mppoint2& C)
{
	using namespace geometrix;
	mpvector2 x0 = mpvector2{ B[0] - A[0], B[1] - A[1] };
	mpvector2 x1 = mpvector2{ C[0] - A[0], C[1] - A[1] };
	return get_Orientation(x0, x1);
}

TEST_F(exact_test_suite, simple)
{
	using namespace stk;
	using namespace geometrix;
	using namespace exact;

	auto mp1 = mppoint2(7.3000000000000194, 7.3000000000000167 );
	auto mp2 = mppoint2(24.000000000000068, 24.000000000000071 );
	auto mp3 = mppoint2(24.00000000000005, 24.000000000000053 );
	auto mp4 = mppoint2(0.50000000000001621, 0.50000000000001243 );
	
	auto p1 = point2(7.3000000000000194 * boost::units::si::meters, 7.3000000000000167 * boost::units::si::meters);
	auto p2 = point2(24.000000000000068 * boost::units::si::meters, 24.000000000000071 * boost::units::si::meters);
	auto p3 = point2(24.00000000000005 * boost::units::si::meters, 24.000000000000053 * boost::units::si::meters);
	auto p4 = point2(0.50000000000001621 * boost::units::si::meters, 0.50000000000001243 * boost::units::si::meters);
	EXPECT_GT(orientation(p1, p2, p3), 0);
	EXPECT_GT(orientation(p1, p2, p4), 0);
	EXPECT_GT(orientation(p2, p3, p4), 0);

	//auto go = get_orientation(p3, p1, p4, direct_comparison_policy());
	auto eo = orientation(p3, p1, p4);
	auto mpo = get_Orientation(mp3, mp1, mp4);
	EXPECT_EQ(eo, mpo);
}
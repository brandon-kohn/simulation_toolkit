//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <stk/geometry/geometry_kernel.hpp>
#include <stk/geometry/transformer.hpp>

TEST(TransformerTestSuite, testBasicUsage)
{
    using namespace ::testing;
    using namespace stk;
    using namespace geometrix;

    point2 op{ 1.0 * boost::units::si::meters, 1.0 * boost::units::si::meters };
    point2 dp{ -1.0 * boost::units::si::meters, 1.0 * boost::units::si::meters };
    point2 dr{ 0.0 * boost::units::si::meters, 0.0 * boost::units::si::meters };
    units::angle theta = geometrix::constants::half_pi<units::angle>();

    transformer2 sut;
    sut.translate(dp).rotate(theta).translate(-as_vector(op));

    auto r = sut(op);   
    EXPECT_TRUE(numeric_sequence_equals_2d(r, dp, make_tolerance_policy()));

    auto r2 = sut(point2{ 0.5 * boost::units::si::meters, 0.5 * boost::units::si::meters });
    EXPECT_TRUE(numeric_sequence_equals_2d(r2, point2{ -0.5 * boost::units::si::meters, 0.5 * boost::units::si::meters }, make_tolerance_policy()));
}

TEST(TransformerTestSuite, testOffsetRotatedAxes)
{
    using namespace ::testing;
    using namespace stk;
    using namespace geometrix;

    point2 oA{ 0.0 * boost::units::si::meters, 0.0 * boost::units::si::meters };
    point2 oB{ 1.0 * boost::units::si::meters, 1.0 * boost::units::si::meters };
    units::angle theta = 0.25 * geometrix::constants::pi<units::angle>();

    transformer2 sut;
    sut.translate(oB).rotate(theta).translate(-as_vector(oA));

    auto segxA = segment2{ oA, point2{ 1.0 * boost::units::si::meters, 0.0 * boost::units::si::meters } };
    auto segyA = segment2{ oA, point2{ 0.0 * boost::units::si::meters, 1.0 * boost::units::si::meters } };

    auto segxB = sut(segxA);
    auto segyB = sut(segyA);
    
    EXPECT_TRUE(numeric_sequence_equals_2d(segxB.get_start(), point2{ 1.0 * boost::units::si::meters, 1.0 * boost::units::si::meters }, make_tolerance_policy()));
    EXPECT_TRUE(numeric_sequence_equals_2d(segxB.get_end(), point2{ 1.7071067811865475 * boost::units::si::meters, 1.7071067811865475 * boost::units::si::meters }, make_tolerance_policy()));

    EXPECT_TRUE(numeric_sequence_equals_2d(segyB.get_start(), point2{ 1.0 * boost::units::si::meters, 1.0 * boost::units::si::meters }, make_tolerance_policy()));
    EXPECT_TRUE(numeric_sequence_equals_2d(segyB.get_end(), point2{ 0.29289321881345243 * boost::units::si::meters, 1.7071067811865475 * boost::units::si::meters }, make_tolerance_policy()));
}

TEST(TransformerTestSuite, testCase1)
{
    using namespace ::testing;
    using namespace stk;
    using namespace geometrix;
    using std::cos;
    using std::sin;

    point2 op{ -118.04574333498022 * boost::units::si::meters, 9.9930356699042022 * boost::units::si::meters };
    point2 dp{ -118.17108733498026 * boost::units::si::meters, 10.032866669818759 * boost::units::si::meters };
    point2 dr{ -52.078490334970411 * boost::units::si::meters, 18.071499669924378 * boost::units::si::meters };
    //units::angle theta = 0.088582999999999995 * units::si::radians;
    units::angle theta = 0.088582999999999995 * (geometrix::constants::pi<double>() / 180.0) * units::si::radians;

    auto cost = cos(theta);
    auto sint = sin(theta);
    vector2 t = op - op;

    polyline2 from{ { -98.812037174440036 * boost::units::si::meters, 3.704616406039666 * boost::units::si::meters },{ -98.730050000012852 * boost::units::si::meters, 8.6038500005379319 * boost::units::si::meters },{ -98.730485487279253 * boost::units::si::meters, 8.6038531885851981 * boost::units::si::meters } };
    polyline2 to{ { -98.723170052621242 * boost::units::si::meters, 8.4114582485210363 * boost::units::si::meters },{ -98.81315000000177 * boost::units::si::meters, 3.5125500001013279 * boost::units::si::meters },{ -98.812037060730916 * boost::units::si::meters, 3.5125469878725553 * boost::units::si::meters } };

    transformer2 sut;
    sut.translate(dp).rotate(theta).translate(-as_vector(op));

    auto r = sut(op);
    auto r2 = sut(from[0]);
    
    EXPECT_TRUE(numeric_sequence_equals_2d(r, dp, make_tolerance_policy()));
}

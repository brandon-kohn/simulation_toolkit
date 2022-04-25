#include <stk/geometry/geometry_kernel.hpp>
#include <stk/geometry/serialization.hpp>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include <sstream>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

TEST(serialization_test_suite, serialize_point)
{
	std::stringstream str;

	boost::archive::text_oarchive out( str );

	auto p = stk::point2{ 5.4 * boost::units::si::meters, 3.2 * boost::units::si::meters };

	out & p;

	str.seekg( 0 );
	boost::archive::text_iarchive in( str );
	p = stk::point2{};

	in & p;

	auto cmp = stk::make_tolerance_policy();

	EXPECT_TRUE( cmp.equals(p[0].value(), 5.4) );
	EXPECT_TRUE( cmp.equals(p[1].value(), 3.2) );
}

TEST(serialization_test_suite, serialize_segment)
{
	std::stringstream str;

	boost::archive::text_oarchive out( str );

	auto p0 = stk::point2{ 5.4 * boost::units::si::meters, 3.2 * boost::units::si::meters };
	auto p1 = stk::point2{ 6.4 * boost::units::si::meters, 4.2 * boost::units::si::meters };

	auto s = stk::segment2{ p0, p1 };

	out & s;

	str.seekg( 0 );
	boost::archive::text_iarchive in( str );
	s = stk::segment2{};

	in & s;

	auto cmp = stk::make_tolerance_policy();

	EXPECT_TRUE( geometrix::numeric_sequence_equals(s.get_start(), p0, cmp) );
	EXPECT_TRUE( geometrix::numeric_sequence_equals(s.get_end(), p1, cmp) );
}

TEST(serialization_test_suite, serialize_aabb )
{
	std::stringstream str;

	boost::archive::text_oarchive out( str );

	auto p0 = stk::point2{ 5.4 * boost::units::si::meters, 3.2 * boost::units::si::meters };
	auto p1 = stk::point2{ 6.4 * boost::units::si::meters, 4.2 * boost::units::si::meters };

	auto s = stk::aabb2{ p0, p1 };

	out & s;

	str.seekg( 0 );
	boost::archive::text_iarchive in( str );
	s = stk::aabb2{ p1, p0 };

	in & s;

	auto cmp = stk::make_tolerance_policy();

	EXPECT_TRUE( geometrix::numeric_sequence_equals(s.get_lower_bound(), p0, cmp) );
	EXPECT_TRUE( geometrix::numeric_sequence_equals(s.get_upper_bound(), p1, cmp) );
}

TEST(serialization_test_suite, serialize_polyline)
{
	std::stringstream str;

	boost::archive::text_oarchive out( str );

	auto p0 = stk::point2{ 5.4 * boost::units::si::meters, 3.2 * boost::units::si::meters };
	auto p1 = stk::point2{ 6.4 * boost::units::si::meters, 4.2 * boost::units::si::meters };
	auto p2 = stk::point2{ 8.4 * boost::units::si::meters, 1.2 * boost::units::si::meters };

	stk::polyline2 pline = { p0, p1, p2 };

	out & pline;

	str.seekg( 0 );
	boost::archive::text_iarchive in( str );
	pline = stk::polyline2{};

	in & pline;

	auto cmp = stk::make_tolerance_policy();

	EXPECT_TRUE( geometrix::numeric_sequence_equals(pline[0], p0, cmp) );
	EXPECT_TRUE( geometrix::numeric_sequence_equals(pline[1], p1, cmp) );
	EXPECT_TRUE( geometrix::numeric_sequence_equals(pline[2], p2, cmp) );
}

TEST(serialization_test_suite, serialize_polygon)
{
	std::stringstream str;

	boost::archive::text_oarchive out( str );

	auto p0 = stk::point2{ 5.4 * boost::units::si::meters, 3.2 * boost::units::si::meters };
	auto p1 = stk::point2{ 6.4 * boost::units::si::meters, 4.2 * boost::units::si::meters };
	auto p2 = stk::point2{ 8.4 * boost::units::si::meters, 1.2 * boost::units::si::meters };

	stk::polygon2 pgon = { p0, p1, p2 };

	out & pgon;

	str.seekg( 0 );
	boost::archive::text_iarchive in( str );
	pgon = stk::polygon2{};

	in & pgon;

	auto cmp = stk::make_tolerance_policy();

	EXPECT_TRUE( geometrix::numeric_sequence_equals(pgon[0], p0, cmp) );
	EXPECT_TRUE( geometrix::numeric_sequence_equals(pgon[1], p1, cmp) );
	EXPECT_TRUE( geometrix::numeric_sequence_equals(pgon[2], p2, cmp) );
}

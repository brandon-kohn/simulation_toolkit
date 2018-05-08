//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#define _ENABLE_ATOMIC_ALIGNMENT_FIX

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <clipper/clipper.hpp>
#include <poly2tri/poly2tri.h>
#include <stk/geometry/geometry_kernel.hpp>
#include <stk/geometry/space_partition/rtree_triangle_cache.ipp>
#include <stk/geometry/apply_unit.hpp>

TEST(RTreeTriangleCacheTestSuite, testBasicUsage_2TrianglesInMesh_PointInBothTrianglesBoundingBox)
{
    using namespace ::testing;
    using namespace stk;

    std::vector<point2> points = apply_unit<std::vector<point2>>({ { 0.0, 0.0 },{ 10.0, 0.0 },{ 10.0, 10.0 },{ 0.0, 10.0 } }, units::si::meters);
    std::vector<std::array<point2, 3>> trigs = { { points[0], points[1], points[2] },{ points[0], points[2], points[3] } };

    rtree_triangle_cache sut(points, trigs);

    point2 p = apply_unit<point2>({ 2.0, 3.0 }, units::si::meters);

    auto result = sut.find_indices(p);

    EXPECT_THAT(result, ElementsAre(0, 1));//! Note, bounding box is used.. so both have the same box.
}

TEST(RTreeTriangleCacheTestSuite, testBasicUsage_2TrianglesInMesh_PointOutside)
{
    using namespace ::testing;
    using namespace stk;
    
    std::vector<point2> points = apply_unit<std::vector<point2>>({ { 0.0, 0.0 },{ 10.0, 0.0 },{ 10.0, 10.0 },{ 0.0, 10.0 } }, units::si::meters);
    std::vector<std::array<point2, 3>> trigs = { { points[0], points[1], points[2] },{ points[0], points[2], points[3] } };

    rtree_triangle_cache sut(points, trigs);

    point2 p = apply_unit<point2>({ -2.0, 3.0 }, units::si::meters);

    auto result = sut.find_indices(p);

    EXPECT_TRUE(result.empty());
}

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <stk/utility/compressed_integer_pair.hpp>
TEST(compressed_integer_pair_tests, compressed_integer_as_8_bytes)
{
	using namespace stk;
	std::uint32_t hiword = 10;
	std::uint32_t loword = 10;
	compressed_integer_pair p = { hiword, loword };
	std::uint64_t val = (std::uint64_t)hiword << 32 | loword;
	EXPECT_EQ(val, p.to_uint64());
}

TEST(compressed_integer_pair_tests, atomic_is_lock_free)
{
	using namespace stk;
	std::uint32_t hiword = 10;
	std::uint32_t loword = 10;
	std::atomic<compressed_integer_pair> p{compressed_integer_pair{ hiword, loword }};
	EXPECT_TRUE(p.is_lock_free());
}

TEST(compressed_integer_pair_tests, atomic_cas_works)
{
	using namespace stk;
	std::uint32_t hiword = 10;
	std::uint32_t loword = 20;
	auto a = compressed_integer_pair{ hiword, loword };

	std::atomic<compressed_integer_pair> p{a};
	auto b = compressed_integer_pair{ loword, hiword };

	EXPECT_TRUE(p.compare_exchange_strong(a, b));
	EXPECT_EQ(b, p.load());
}



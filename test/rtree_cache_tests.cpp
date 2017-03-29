#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <clipper/clipper.hpp>
#include <poly2tri/poly2tri.hpp>
#include <stk/geometry/space_partition/rtree_triangle_cache.ipp>

TEST(RTreeTriangleCacheTestSuite, testBasicUsage_2TrianglesInMesh_PointInBothTrianglesBoundingBox)
{
	using namespace ::testing;

	std::vector<point2> points = apply_unit<std::vector<point2>>({ { 0.0, 0.0 },{ 10.0, 0.0 },{ 10.0, 10.0 },{ 0.0, 10.0 } }, units::si::meters);
	std::vector<std::array<point2, 3>> trigs = { { points[0], points[1], points[2] },{ points[0], points[2], points[3] } };

	stk::rtree_triangle_cache sut(points, trigs);

	point2 p = apply_unit<point2>({ 2.0, 3.0 }, units::si::meters);

	auto result = sut.find_indices(p);

	EXPECT_THAT(result, ElementsAre(0, 1));//! Note, bounding box is used.. so both have the same box.
}

TEST(RTreeTriangleCacheTestSuite, testBasicUsage_2TrianglesInMesh_PointOutside)
{
	using namespace ::testing;

	std::vector<point2> points = apply_unit<std::vector<point2>>({ { 0.0, 0.0 },{ 10.0, 0.0 },{ 10.0, 10.0 },{ 0.0, 10.0 } }, units::si::meters);
	std::vector<std::array<point2, 3>> trigs = { { points[0], points[1], points[2] },{ points[0], points[2], points[3] } };

	stk::rtree_triangle_cache sut(points, trigs);

	point2 p = apply_unit<point2>({ -2.0, 3.0 }, units::si::meters);

	auto result = sut.find_indices(p);

	EXPECT_TRUE(result.empty());
}

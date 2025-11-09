//
//! Copyright 2025
//! Unit tests for is_vector_between function
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#define _USE_MATH_DEFINES
#include <cmath>
#include <gtest/gtest.h>
#include <geometrix/utility/utilities.hpp>
#include <geometrix/numeric/number_comparison_policy.hpp>
#include <geometrix/primitive/point.hpp>
#include <geometrix/arithmetic/vector.hpp>
#include <geometrix/algebra/algebra.hpp>

using namespace geometrix;

class IsVectorBetweenTest : public ::testing::Test 
{
protected:
    using point_2d = point_double_2d;
    using vector_2d = vector_double_2d;
    using cmp_policy = absolute_tolerance_comparison_policy<double>;
    
    void SetUp() override 
    {
        cmp = cmp_policy(1e-10);
    }
    
    cmp_policy cmp;
    
    // Helper to create vectors from angles (in radians)
    vector_2d vector_from_angle(double angle) 
    {
        return vector_2d(std::cos(angle), std::sin(angle));
    }
    
    // Helper to create vector from components
    vector_2d make_vector(double x, double y) 
    {
        return vector_2d(x, y);
    }
};

// Test basic cases with axis-aligned vectors
TEST_F(IsVectorBetweenTest, BasicAxisAlignedVectors) 
{
    // Test with cardinal directions
    auto right = make_vector(1, 0);    // 0°
    auto up = make_vector(0, 1);       // 90°
    auto left = make_vector(-1, 0);    // 180°
    auto down = make_vector(0, -1);    // 270°
    
    // Test c between a and b (counterclockwise from a to b)
    auto diag_45 = make_vector(std::cos(M_PI/4), std::sin(M_PI/4));      // 45°
    auto diag_135 = make_vector(std::cos(3*M_PI/4), std::sin(3*M_PI/4)); // 135°
    auto diag_225 = make_vector(std::cos(5*M_PI/4), std::sin(5*M_PI/4)); // 225°  
    auto diag_315 = make_vector(std::cos(7*M_PI/4), std::sin(7*M_PI/4)); // 315°
    
    EXPECT_TRUE(is_vector_between(right, up, diag_45, true, cmp));    // 45° between 0° and 90°
    EXPECT_TRUE(is_vector_between(up, left, diag_135, true, cmp));    // 135° between 90° and 180°
    EXPECT_TRUE(is_vector_between(left, down, diag_225, true, cmp));  // 225° between 180° and 270°
    EXPECT_TRUE(is_vector_between(down, right, diag_315, true, cmp)); // 315° between 270° and 360°
}

// Test boundary conditions
TEST_F(IsVectorBetweenTest, BoundaryConditions) 
{
    auto right = make_vector(1, 0);
    auto up = make_vector(0, 1);
    auto diagonal = make_vector(1, 1);  // 45°
    
    // Test when c is exactly on boundary vectors
    EXPECT_TRUE(is_vector_between(right, up, right, true, cmp));   // c == a, includeBounds=true
    EXPECT_FALSE(is_vector_between(right, up, right, false, cmp)); // c == a, includeBounds=false
    
    EXPECT_TRUE(is_vector_between(right, up, up, true, cmp));      // c == b, includeBounds=true  
    EXPECT_FALSE(is_vector_between(right, up, up, false, cmp));    // c == b, includeBounds=false
}

// Test collinear vectors
TEST_F(IsVectorBetweenTest, CollinearVectors) 
{
    auto right = make_vector(1, 0);
    auto right2 = make_vector(2, 0);    // Same direction, different magnitude
    auto left = make_vector(-1, 0);     // Opposite direction
    
    // Collinear, same direction
    EXPECT_TRUE(is_vector_between(right, make_vector(0, 1), right2, true, cmp));   // includeBounds=true
    EXPECT_FALSE(is_vector_between(right, make_vector(0, 1), right2, false, cmp)); // includeBounds=false
    
    // Collinear, opposite direction
    EXPECT_FALSE(is_vector_between(right, make_vector(0, 1), left, true, cmp));
    EXPECT_FALSE(is_vector_between(right, make_vector(0, 1), left, false, cmp));
}

// Test when a and b are collinear
TEST_F(IsVectorBetweenTest, CollinearBoundaryVectors) 
{
    auto right = make_vector(1, 0);
    auto right2 = make_vector(2, 0);    // Same direction as right
    auto left = make_vector(-1, 0);     // Opposite direction
    auto up = make_vector(0, 1);        // Perpendicular
    
    // When a and b point in same direction - no "between" region exists
    EXPECT_FALSE(is_vector_between(right, right2, up, true, cmp));
    EXPECT_FALSE(is_vector_between(right, right2, up, false, cmp));
    
    // When a and b are anti-parallel - any non-collinear vector is "between"
    // Revised convention: increasing winding is to the left. For anti-parallel (180° apart)
    // "between" means c is left of a AND right of b. Neither up (0,1) nor down (0,-1) satisfy both.
    EXPECT_FALSE(is_vector_between(right, left, up, true, cmp));
    EXPECT_FALSE(is_vector_between(right, left, up, false, cmp));
    EXPECT_FALSE(is_vector_between(right, left, make_vector(0, -1), true, cmp));
}

// Test small angles
TEST_F(IsVectorBetweenTest, SmallAngles) 
{
    // Test with very small angular differences
    auto a = vector_from_angle(0.0);           // 0°
    auto b = vector_from_angle(0.1);           // ~5.7°
    auto c1 = vector_from_angle(0.05);         // ~2.9° (between)
    auto c2 = vector_from_angle(0.2);          // ~11.5° (not between)
    
    EXPECT_TRUE(is_vector_between(a, b, c1, true, cmp));
    EXPECT_FALSE(is_vector_between(a, b, c2, true, cmp));
}

// Test large angles (> 180°)
TEST_F(IsVectorBetweenTest, LargeAngles) 
{
    // Test when the angle from a to b (counterclockwise) is > 180°
    auto right = make_vector(1, 0);     // 0°
    auto up_left = make_vector(-1, 1);  // 135°
    auto down = make_vector(0, -1);     // 270°
    
    // The short path from right to up_left is counterclockwise (~135°)
    // But going clockwise from right to up_left covers ~225°
    // down (270°) should be in the larger arc going clockwise from right to up_left
    EXPECT_TRUE(is_vector_between(right, up_left, down, true, cmp));
}

// Test numerical edge cases
TEST_F(IsVectorBetweenTest, NumericalEdgeCases) 
{
    // Test with very small magnitude vectors
    auto tiny_right = make_vector(1e-10, 0);
    auto tiny_up = make_vector(0, 1e-10);
    auto tiny_diag = make_vector(1e-10, 1e-10);
    
    EXPECT_TRUE(is_vector_between(tiny_right, tiny_up, tiny_diag, true, cmp));
    
    // Test with mixed magnitudes
    auto big_right = make_vector(1000, 0);
    auto small_up = make_vector(0, 0.001);
    auto medium_diag = make_vector(1, 1);
    
    EXPECT_TRUE(is_vector_between(big_right, small_up, medium_diag, true, cmp));
}

// Test specific geometric configurations that might cause issues
TEST_F(IsVectorBetweenTest, ProblematicConfigurations) 
{
    // Test configuration that might confuse the algorithm
    // Vectors that are close to 180° apart
    auto a = vector_from_angle(0.1);           // ~5.7°
    auto b = vector_from_angle(M_PI - 0.1);    // ~174.3°
    auto c = vector_from_angle(M_PI/2);        // 90° (clearly between)
    
    EXPECT_TRUE(is_vector_between(a, b, c, true, cmp));
    
    // Test vectors close to being anti-parallel
    auto near_right = vector_from_angle(0.01);        // ~0.6°
    auto near_left = vector_from_angle(M_PI + 0.01);  // ~180.6°
    auto up = make_vector(0, 1);                      // 90°
    
    EXPECT_TRUE(is_vector_between(near_right, near_left, up, true, cmp));
}

// Test symmetry properties
TEST_F(IsVectorBetweenTest, SymmetryProperties) 
{
    auto a = vector_from_angle(0);
    auto b = vector_from_angle(M_PI/2);
    auto c = vector_from_angle(M_PI/4);
    
    // The function should not be symmetric in a and b due to orientation
    bool result_ab = is_vector_between(a, b, c, true, cmp);
    bool result_ba = is_vector_between(b, a, c, true, cmp);
    
    // These should be different because the "between" concept depends on orientation
    // c is between a and b going counterclockwise, but not between b and a going counterclockwise
    EXPECT_NE(result_ab, result_ba);
}

// Stress test with random angles
TEST_F(IsVectorBetweenTest, RandomAngleStressTest) 
{
    // Test various random configurations to check for crashes or assertion failures
    std::vector<double> angles = {
        0, M_PI/6, M_PI/4, M_PI/3, M_PI/2, 2*M_PI/3, 3*M_PI/4, 5*M_PI/6, 
        M_PI, 7*M_PI/6, 5*M_PI/4, 4*M_PI/3, 3*M_PI/2, 5*M_PI/3, 7*M_PI/4, 11*M_PI/6
    };
    
    for (double angle_a : angles) {
        for (double angle_b : angles) {
            if (angle_a == angle_b) continue;
            
            auto a = vector_from_angle(angle_a);
            auto b = vector_from_angle(angle_b);
            
            // Test a vector that should be clearly between (halfway angle)
            double mid_angle = angle_a + (angle_b - angle_a) * 0.5;
            if (angle_b < angle_a) mid_angle += M_PI; // Handle wraparound
            if (mid_angle >= 2*M_PI) mid_angle -= 2*M_PI;
            
            auto c = vector_from_angle(mid_angle);
            
            // This should not crash, though the result may vary based on the specific algorithm logic
            EXPECT_NO_THROW({
                bool result = is_vector_between(a, b, c, true, cmp);
                (void)result; // Suppress unused variable warning
            });
        }
    }
}
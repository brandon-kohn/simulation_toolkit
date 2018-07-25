//
//! Copyright © 2018
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <stk/utility/colors.hpp>
#include <geometrix/algebra/expression.hpp>

TEST(color_test_suite, construct_color)
{
    using namespace stk;

    auto c = color_rgba{255, 0, 0};
    EXPECT_EQ(c, colors::Red);
}

TEST(color_test_suite, access_channels)
{
    using namespace stk;

    auto c = color_rgba{255, 0, 0};
    EXPECT_EQ(255, c.red);
    EXPECT_EQ(0, c.green);
    EXPECT_EQ(0, c.blue);
    EXPECT_EQ(255, c.alpha);
}

TEST(color_test_suite, construct_from_expression)
{
    using namespace stk;
    using namespace geometrix;

    auto r = colors::Red;
    auto g = colors::Lime;
    auto v = color_vector(g - r);
    auto c = color_rgba(r + v);
    EXPECT_EQ(c, colors::Lime);
}

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

#include <clipper/clipper.hpp>

TEST(ClipperSuite, testCriticalPolygon)
{
    using namespace ::testing;

    ClipperLib::Path path;

    path << ClipperLib::IntPoint( 25170, -4640 );
    path << ClipperLib::IntPoint( 24700, -4707 );
    path << ClipperLib::IntPoint( 24231, -4795 );
    path << ClipperLib::IntPoint( 23765, -4926 );
    path << ClipperLib::IntPoint( 23323, -5142 );
    path << ClipperLib::IntPoint( 22917, -5392 );
    path << ClipperLib::IntPoint( 22585, -5709 );
    path << ClipperLib::IntPoint( 22346, -6222 );
    path << ClipperLib::IntPoint( 22167, -6750 );
    path << ClipperLib::IntPoint( 22096, -7164 );
    path << ClipperLib::IntPoint( 22059, -7586 );
    path << ClipperLib::IntPoint( 22104, -10396 );
    path << ClipperLib::IntPoint( 12375, -10320 );
    path << ClipperLib::IntPoint( 12348, -8363 );
    path << ClipperLib::IntPoint( 12333, -7950 );
    path << ClipperLib::IntPoint( 12278, -7548 );
    path << ClipperLib::IntPoint( 11822, -6740 );
    path << ClipperLib::IntPoint( 11569, -6360 );
    path << ClipperLib::IntPoint( 11286, -6007 );
    path << ClipperLib::IntPoint( 10918, -5646 );
    path << ClipperLib::IntPoint( 10510, -5321 );
    path << ClipperLib::IntPoint( 10077, -5021 );
    path << ClipperLib::IntPoint( 9630, -4732 );
    path << ClipperLib::IntPoint( 9153, -4639 );
    path << ClipperLib::IntPoint( 8653, -4603 );
    path << ClipperLib::IntPoint( 8565, 17141 );
    path << ClipperLib::IntPoint( 11546, 17108 );
    path << ClipperLib::IntPoint( 13531, 17056 );
    path << ClipperLib::IntPoint( 15512, 16959 );
    path << ClipperLib::IntPoint( 16994, 16846 );
    path << ClipperLib::IntPoint( 18435, 16694 );
    path << ClipperLib::IntPoint( 20254, 16453 );
    path << ClipperLib::IntPoint( 25190, 15694 );

    ClipperLib::Clipper pc;
    ClipperLib::ClipperOffset co;
    co.AddPath(path, ClipperLib::jtSquare, ClipperLib::etClosedPolygon);
    {
        ClipperLib::Paths solution1;
        co.Execute( solution1, 100 );
        if( !solution1.empty() ){
            pc.AddPath(solution1.at(0), ClipperLib::ptClip, true);
        }
    }
}

//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_SIGNED_DISTANCE_HPP
#define STK_SIGNED_DISTANCE_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include "geometry_kernel.hpp"

inline units::length signed_distance_point_line(const point2& p, const segment2& seg)
{
    using namespace geometrix;
    vector2 segVector = seg.get_end() - seg.get_start();
    return scalar_projection(p - seg.get_start(), left_normal(normalize(segVector)));
}

inline units::length signed_distance_point_line(const point2& p, const line2& l)
{
    using namespace geometrix;
    return scalar_projection(p - l.get_u(), left_normal(l.get_parallel_vector()));
}

#endif//STK_SIGNED_DISTANCE_HPP

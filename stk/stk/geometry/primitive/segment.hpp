//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_SEGMENT_HPP
#define STK_SEGMENT_HPP
#pragma once

#include "geometry_point.hpp"

#include <geometrix/primitive/segment.hpp>
using segment2 = geometrix::segment<point2>;

//! Comparison for point2 types.
namespace geometrix{
    
    inline bool operator <(const segment2& lhs, const segment2& rhs)
    {
        auto cmp = make_tolerance_policy();
        if( geometrix::lexicographically_less_than( lhs.get_start(), rhs.get_start(), cmp ) ) {
            return true;
        }

        if( geometrix::numeric_sequence_equals_2d( lhs.get_start(), rhs.get_start(), cmp ) ) {
            return geometrix::lexicographically_less_than( lhs.get_end(), rhs.get_end(), cmp );
        }

        return false;
    }
    
    inline bool operator ==(const segment2& lhs, const segment2& rhs)
    {
        auto cmp = make_tolerance_policy();
        return geometrix::numeric_sequence_equals_2d( lhs.get_start(), rhs.get_start(), cmp ) && geometrix::numeric_sequence_equals_2d( lhs.get_end(), rhs.get_end(), cmp );
    }
}//! namespace geometrix


#endif//STK_SEGMENT_HPP

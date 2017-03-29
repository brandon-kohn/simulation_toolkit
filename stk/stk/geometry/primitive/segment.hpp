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

#if defined(_MSC_VER)
    #pragma once
#endif

#include "point.hpp"

#include <geometrix/primitive/segment.hpp>

namespace stk {
    
using segment2 = geometrix::segment<point2>;

}//! namespace stk;

//! Comparison for point2 types.
namespace geometrix {
    
    inline bool operator <(const stk::segment2& lhs, const stk::segment2& rhs)
    {
        using namespace stk;
        auto cmp = make_tolerance_policy();
        if( lexicographically_less_than( lhs.get_start(), rhs.get_start(), cmp ) ) {
            return true;
        }

        if( numeric_sequence_equals_2d( lhs.get_start(), rhs.get_start(), cmp ) ) {
            return lexicographically_less_than( lhs.get_end(), rhs.get_end(), cmp );
        }

        return false;
    }
    
    inline bool operator ==(const stk::segment2& lhs, const stk::segment2& rhs)
    {
        using namespace stk;        
        auto cmp = make_tolerance_policy();
        return numeric_sequence_equals_2d( lhs.get_start(), rhs.get_start(), cmp ) && numeric_sequence_equals_2d( lhs.get_end(), rhs.get_end(), cmp );
    }
}//! namespace geometrix

#endif//STK_SEGMENT_HPP

//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_POINT_HPP
#define STK_POINT_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include <stk/units/boost_units.hpp>
#include <stk/geometry/tolerance_policy.hpp>

#include <geometrix/utility/utilities.hpp>
#include <geometrix/arithmetic/boost_units_arithmetic.hpp>
#include <geometrix/space/neutral_reference_frame.hpp>
#include <geometrix/tensor/index_operator_vector_access_policy.hpp>
#include <geometrix/primitive/point.hpp>

namespace stk {

using point2 = geometrix::point<units::length,2>;
using point3 = geometrix::point<units::length,3>;

}//! namespace stk;

GEOMETRIX_DEFINE_POINT_TRAITS(stk::point2, (stk::units::length), 2, stk::units::dimensionless, stk::units::length, neutral_reference_frame_2d, index_operator_vector_access_policy<stk::point2>);
GEOMETRIX_DEFINE_POINT_TRAITS(stk::point3, (stk::units::length), 3, stk::units::dimensionless, stk::units::length, neutral_reference_frame_3d, index_operator_vector_access_policy<stk::point3>);

namespace geometrix {
    inline bool operator <(const stk::point2& lhs, const stk::point2& rhs)
    {
        return geometrix::lexicographically_less_than( lhs, rhs, stk::make_tolerance_policy() );
    }
    
    inline bool operator ==(const stk::point2& lhs, const stk::point2& rhs)
    {
        return geometrix::numeric_sequence_equals_2d( lhs, rhs, stk::make_tolerance_policy() );
    }
}//! namespace geometrix

#endif//STK_POINT_HPP
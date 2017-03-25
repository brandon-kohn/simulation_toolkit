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
#pragma once

#include <stk/units/boost_units.hpp>
#include <stk/geometry/tolerance_policy.hpp>

#include <geometrix/utility/utilities.hpp>
#include <geometrix/arithmetic/boost_units_arithmetic.hpp>
#include <geometrix/space/neutral_reference_frame.hpp>
#include <geometrix/tensor/index_operator_vector_access_policy.hpp>
#include <geometrix/primitive/point.hpp>

using point2 = geometrix::point<units::length,2>;
using point3 = geometrix::point<units::length,3>;
GEOMETRIX_DEFINE_POINT_TRAITS(point2, (units::length), 2, units::dimensionless, units::length, neutral_reference_frame_2d, index_operator_vector_access_policy<point2>);
GEOMETRIX_DEFINE_POINT_TRAITS(point3, (units::length), 3, units::dimensionless, units::length, neutral_reference_frame_3d, index_operator_vector_access_policy<point3>);

namespace geometrix {
    inline bool operator <(const point2& lhs, const point2& rhs)
    {
        return geometrix::lexicographically_less_than( lhs, rhs, make_tolerance_policy() );
    }
    
    inline bool operator ==(const point2& lhs, const point2& rhs)
    {
        return geometrix::numeric_sequence_equals_2d( lhs, rhs, make_tolerance_policy() );
    }
}//! namespace geometrix

#endif//STK_POINT_HPP
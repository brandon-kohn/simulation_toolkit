//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_VECTOR_HPP
#define STK_VECTOR_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include <stk/units/boost_units.hpp>
#include <geometrix/arithmetic/boost_units_arithmetic.hpp>
#include <geometrix/space/neutral_reference_frame.hpp>
#include <geometrix/tensor/index_operator_vector_access_policy.hpp>

#include <geometrix/tensor/vector.hpp>

namespace stk {

using vector2 = geometrix::vector<units::length, 2>;
using vector3 = geometrix::vector<units::length, 3>;
using dimensionless2 = geometrix::vector<units::dimensionless, 2>;
using velocity2 = geometrix::vector<units::speed, 2>;
using acceleration2 = geometrix::vector<units::acceleration, 2>;
using force2 = geometrix::vector<units::force, 2>;
using momentum2 = geometrix::vector<units::momentum, 2>;

template <typename T>
inline dimensionless2 make_dimensionless(const T& expr)
{
    using namespace geometrix;
    return dimensionless2{ get<0>(expr).value(), get<1>(expr).value() };
}

}//! namespace stk;

GEOMETRIX_DEFINE_VECTOR_TRAITS(stk::vector2, (stk::units::length), 2, stk::units::dimensionless, stk::units::length, neutral_reference_frame_2d, index_operator_vector_access_policy<stk::vector2>);
GEOMETRIX_DEFINE_VECTOR_TRAITS(stk::vector3, (stk::units::length), 3, stk::units::dimensionless, stk::units::length, neutral_reference_frame_3d, index_operator_vector_access_policy<stk::vector3>);
GEOMETRIX_DEFINE_VECTOR_TRAITS(stk::dimensionless2, (stk::units::dimensionless), 2, stk::units::dimensionless, stk::units::dimensionless, neutral_reference_frame_2d, index_operator_vector_access_policy<stk::dimensionless2>);
GEOMETRIX_DEFINE_VECTOR_TRAITS(stk::velocity2, (stk::units::speed), 2, stk::units::dimensionless, stk::units::speed, neutral_reference_frame_2d, index_operator_vector_access_policy<stk::velocity2>);
GEOMETRIX_DEFINE_VECTOR_TRAITS(stk::acceleration2, (stk::units::acceleration), 2, stk::units::dimensionless, stk::units::acceleration, neutral_reference_frame_2d, index_operator_vector_access_policy<stk::acceleration2>);
GEOMETRIX_DEFINE_VECTOR_TRAITS(stk::force2, (stk::units::force), 2, stk::units::dimensionless, stk::units::force, neutral_reference_frame_2d, index_operator_vector_access_policy<stk::force2>);
GEOMETRIX_DEFINE_VECTOR_TRAITS(stk::momentum2, (stk::units::momentum), 2, stk::units::dimensionless, stk::units::momentum, neutral_reference_frame_2d, index_operator_vector_access_policy<stk::momentum2>);

#endif//STK_VECTOR_HPP

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
namespace geometry_kernel_detail {
    using vector2 = geometrix::vector<units::length, 2>;//! define this one in a namespace to avoid confusion when specialized for boost.fusion (confusion with boost::fusion::vector2.)
    using vector3 = geometrix::vector<units::length, 3>;//! define this one in a namespace to avoid confusion when specialized for boost.fusion (confusion with boost::fusion::vector3.)
}
using vector2 = ::geometry_kernel_detail::vector2;
GEOMETRIX_DEFINE_VECTOR_TRAITS(::geometry_kernel_detail::vector2, (units::length), 2, units::dimensionless, units::length, neutral_reference_frame_2d, index_operator_vector_access_policy<::geometry_kernel_detail::vector2>);

using vector3 = ::geometry_kernel_detail::vector3;
GEOMETRIX_DEFINE_VECTOR_TRAITS(::geometry_kernel_detail::vector3, (units::length), 3, units::dimensionless, units::length, neutral_reference_frame_3d, index_operator_vector_access_policy<::geometry_kernel_detail::vector3>);

using dimensionless2 = geometrix::vector<units::dimensionless, 2>;
GEOMETRIX_DEFINE_VECTOR_TRAITS(dimensionless2, (units::dimensionless), 2, units::dimensionless, units::dimensionless, neutral_reference_frame_2d, index_operator_vector_access_policy<dimensionless2>);

using velocity2 = geometrix::vector<units::speed, 2>;
GEOMETRIX_DEFINE_VECTOR_TRAITS(velocity2, (units::speed), 2, units::dimensionless, units::speed, neutral_reference_frame_2d, index_operator_vector_access_policy<velocity2>);

using acceleration2 = geometrix::vector<units::acceleration, 2>;
GEOMETRIX_DEFINE_VECTOR_TRAITS(acceleration2, (units::acceleration), 2, units::dimensionless, units::acceleration, neutral_reference_frame_2d, index_operator_vector_access_policy<acceleration2>);

using force2 = geometrix::vector<units::force, 2>;
GEOMETRIX_DEFINE_VECTOR_TRAITS(force2, (units::force), 2, units::dimensionless, units::force, neutral_reference_frame_2d, index_operator_vector_access_policy<force2>);

using momentum2 = geometrix::vector<units::momentum, 2>;
GEOMETRIX_DEFINE_VECTOR_TRAITS(momentum2, (units::momentum), 2, units::dimensionless, units::momentum, neutral_reference_frame_2d, index_operator_vector_access_policy<momentum2>);

template <typename T>
inline dimensionless2 make_dimensionless(const T& expr)
{
    using namespace geometrix;
    return dimensionless2{ get<0>(expr).value(), get<1>(expr).value() };
}

#endif//STK_VECTOR_HPP

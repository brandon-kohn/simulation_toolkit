//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_GEOMETRY_KERNEL_HPP
#define STK_GEOMETRY_KERNEL_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include <geometrix/utility/assert.hpp>
#include <geometrix/utility/static_assert.hpp>

#include "primitive/point.hpp"
#include "primitive/segment.hpp"
#include "primitive/capsule.hpp"
#include "primitive/polygon.hpp"
#include "primitive/polyline.hpp"
#include "primitive/polygon_with_holes.hpp"
#include "primitive/sphere.hpp"
#include "primitive/line.hpp"
#include "primitive/rectangle.hpp"
#include "primitive/triangle.hpp"
#include "primitive/oriented_bounding_box.hpp"
#include "primitive/axis_aligned_bounding_box.hpp"

#include "tensor/vector.hpp"
#include "tensor/matrix.hpp"

#include "space_partition/bsp_tree.hpp"
#include "space_partition/mesh.hpp"

#include "tolerance_policy.hpp"

//! Load the expression template headers and common algorithms
#include <geometrix/algebra/expression.hpp>
#include <geometrix/primitive/point_sequence_utilities.hpp>
#include <geometrix/algorithm/point_sequence/is_polygon_simple.hpp>
#include <geometrix/algorithm/point_sequence/remove_collinear_points.hpp>
#include <geometrix/utility/utilities.hpp>
#include <geometrix/test/test.hpp>

#include <geometrix/tensor/numeric_sequence.hpp>
#include <geometrix/numeric/boost_units_quantity.hpp>
#include <geometrix/tensor/fusion_vector_access_policy.hpp>
#include <geometrix/utility/member_function_fusion_adaptor.hpp>
#include <geometrix/primitive/array_point_sequence.hpp>
#include <geometrix/primitive/vector_point_sequence.hpp>

#endif//STK_GEOMETRY_KERNEL_HPP

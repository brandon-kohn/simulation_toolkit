//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_MESH_HPP
#define STK_MESH_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include <stk/units/boost_units.hpp>

#include <geometrix/algorithm/mesh_2d.hpp>
#include "rtree_triangle_cache.hpp"

namespace stk {

using mesh2 = geometrix::mesh_2d<stk::units::length, geometrix::mesh_traits<rtree_triangle_cache, geometrix::triangle_area_weight_policy<stk::units::length>>>;

}//! namespace stk;

#endif//STK_MESH_HPP

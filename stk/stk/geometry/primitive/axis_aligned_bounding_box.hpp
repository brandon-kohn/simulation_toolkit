//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_AXIS_ALIGNED_BOUNDING_BOX_HPP
#define STK_AXIS_ALIGNED_BOUNDING_BOX_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include "point.hpp"
#include <geometrix/primitive/axis_aligned_bounding_box.hpp>

namespace stk { 

using aabb2 = geometrix::axis_aligned_bounding_box<point2>;

}//! namespace stk;

#endif//STK_AXIS_ALIGNED_BOUNDING_BOX_HPP

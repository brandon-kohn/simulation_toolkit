//
//! Copyright © 2023
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_RAY_HPP
#define STK_RAY_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include "point.hpp"
#include <stk/geometry/tensor/vector.hpp>

#include <geometrix/primitive/ray.hpp>

namespace stk { 

using ray2 = geometrix::ray<point2>;

}//! namespace stk;

#endif//STK_RAY_HPP

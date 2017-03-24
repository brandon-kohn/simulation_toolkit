//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_SPHERE_HPP
#define STK_SPHERE_HPP
#pragma once

#include "point.hpp"
#include <geometrix/primitive/sphere.hpp>

using circle2 = geometrix::sphere<2, point2>;
using circle3 = geometrix::sphere<2, point3>;
GEOMETRIX_DEFINE_SPHERE_TRAITS(2, point2, circle2);
GEOMETRIX_DEFINE_SPHERE_ACCESS_TRAITS(circle2);

#endif//STK_SPHERE_HPP

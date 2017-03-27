//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_POLYGON_WITH_HOLES_HPP
#define STK_POLYGON_WITH_HOLES_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include "point.hpp"

//! Polygon/Polyline type
#include <geometrix/primitive/polygon_with_holes.hpp>

using polygon_with_holes2 = geometrix::polygon_with_holes<point2>;
using polygon_with_holes3 = geometrix::polygon_with_holes<point3>;

#endif//STK_POLYGON_WITH_HOLES_HPP

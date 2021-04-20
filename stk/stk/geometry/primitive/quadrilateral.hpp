//
//! Copyright © 2021
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_QUAD_HPP
#define STK_QUAD_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include "point.hpp"

#include <geometrix/primitive/quadrilateral.hpp>

namespace stk {

using quad2 = geometrix::quad<point2>;

}//! namespace stk;

#endif//STK_QUAD_HPP

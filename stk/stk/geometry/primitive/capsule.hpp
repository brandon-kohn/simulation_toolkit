//
//! Copyright © 2018
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_CAPSULE_HPP
#define STK_CAPSULE_HPP
#pragma once

#include "point.hpp"
#include <geometrix/primitive/capsule.hpp>

namespace stk { 

using capsule2 = geometrix::capsule<point2>;

}//! namespace stk;

#endif//STK_CAPSULE_HPP

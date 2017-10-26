//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_BSP_TREE_HPP
#define STK_BSP_TREE_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include <stk/geometry/primitive/segment.hpp>
#include <geometrix/algorithm/node_bsp_tree_2d.hpp>

namespace stk {

using bsp_tree2 = geometrix::node_bsp_tree_2d<segment2>;

}//! namespace stk;

#endif//STK_BSP_TREE_HPP

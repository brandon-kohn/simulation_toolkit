//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_MATRIX_HPP
#define STK_MATRIX_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include <geometrix/tensor/matrix.hpp>

namespace stk {

using matrix22 = geometrix::matrix<double, 2, 2>;
using matrix33 = geometrix::matrix<double, 3, 3>;
using matrix44 = geometrix::matrix<double, 4, 4>;

}//! namespace stk;

#endif//STK_MATRIX_HPP

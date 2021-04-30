//
//! Copyright © 2010
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "./detail/unsigned_integer.hpp"

namespace stk {

    //! unsigned_integer<T> is meant to be used as a (nearly) fully operational unsigned integer type.
    //! In general it can be used similar to how a native unsigned type is used with a few caveats:
    //!
    //! std::numeric_limits<T>::max() is used to designate that the UnsignedInteger<T> is in an invalid state.
    //! This happens whenever the value is uninitialized or over/under flows the value of the underlying type T
    //! through arithmetic or increment/decrement operations.
    //! Once the instance reaches an invalid state, it can only be returned to a valid state through assignment.
    using index = unsigned_integer<std::size_t>;
    using index32 = unsigned_integer<std::uint32_t>;
    using index64 = unsigned_integer<std::uint64_t>;

}//! namespace stk;


//
//! Copyright © 2022
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

namespace stk {
    template<typename T, typename... Args>
    inline std::vector<T> make_vector(Args&&... args)
    {
        static_assert((std::is_constructible_v<T, Args&&> && ...));

        std::vector<T> v;
        (v.emplace_back(std::forward<Args>(args)), ...);
        return v;
    }
}//! namespace stk;

//
//! Copyright © 2018
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

namespace stk {

    template <typename T, typename EnableIf=void>
    struct memory_reclamation_traits
    {
        template <typename U, typename Alloc>
        static void reclaim_from_allocator(T& reclaimer, Alloc& alloc, U* ptr, std::size_t n);
    };

}//! namespace stk;


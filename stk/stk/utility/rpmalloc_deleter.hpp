//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once  

#include <rpmalloc/rpmalloc.h>

namespace stk {

    struct rpmalloc_deleter
    {
        template <typename T>
        void operator()(T* ptr)
        {
            rpfree(ptr); 
        }
    };

}//! namespace stk;


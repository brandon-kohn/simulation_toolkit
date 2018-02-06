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
#include <stk/thread/once_block.hpp>
#include <stk/utility/boost_unique_ptr.hpp>

namespace stk {

    struct rpmalloc_system
    {
        static std::unique_ptr<rpmalloc_system> create()
        {                                                     
            return std::unique_ptr<rpmalloc_system>(new rpmalloc_system());
        }                                                     
    
        ~rpmalloc_system()
        {
            STK_ONCE_BLOCK()
            {
                rpmalloc_finalize();
            }
        }

    private:
        rpmalloc_system()
        {
            STK_ONCE_BLOCK()
            {
                rpmalloc_initialize();
            }
        }
    };

}//! namespace stk;

//! This macro creates instance of rpmalloc_system whose scope is static. Should be called on the main thread.
//! Should be called early in the process lifetime.
#define STK_INSTANTIATE_RPMALLOC_SYSTEM()                                                              \
    static auto BOOST_PP_CAT(stk_rpmalloc_system_instance, __LINE__) = stk::rpmalloc_system::create(); \
/***/


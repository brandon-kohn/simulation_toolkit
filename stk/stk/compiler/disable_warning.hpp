//
//! Copyright © 2019
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/config.hpp>
#include <boost/preprocessor/facilities/is_empty.hpp>

#ifndef STK_DISABLE
    #error STK_DISABLE not defined. This should be defined to the warning macro which is to be disabled before including disable_warning.hpp.
#endif

#if(!BOOST_PP_IS_EMPTY(STK_DISABLE))
    #if defined(BOOST_MSVC)
        #pragma warning(disable: STK_DISABLE)
    #elif defined(BOOST_COMP_GNUC) && !defined(BOOST_COMP_CLANG)
        #define STK_PRAGMA_IMPL_(p) _Pragma(#p)
        #define STK_DO_PRAGMA_(p) STK_PRAGMA_IMPL_(p)
        STK_DO_PRAGMA_(GCC diagnostic ignored STK_DISABLE)
        #undef STK_PRAGMA_IMPL_
        #undef STK_DO_PRAGMA_
    #elif defined(BOOST_COMP_CLANG)
        #if defined(__has_warning) 
            #define STK_HAS_WARNING_IMPL_(w) __has_warning(w)
            #if STK_HAS_WARNING_IMPL_(STK_DISABLE)
                #define STK_PRAGMA_IMPL_(p) _Pragma(#p)
                #define STK_DO_PRAGMA_(p) STK_PRAGMA_IMPL_(p)
                STK_DO_PRAGMA_(clang diagnostic ignored STK_DISABLE)
                #undef STK_PRAGMA_IMPL_
                #undef STK_DO_PRAGMA_
            #endif
        #endif
    #else

    #endif//! disable STK_DISABLE
#endif//! If there is a warning.

#ifdef STK_DISABLE
#undef STK_DISABLE
#undef


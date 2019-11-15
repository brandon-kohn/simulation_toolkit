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
    #ifdef STK_WARNING
        #define STK_DISABLE STK_WARNING
    #else
        #error STK_DISABLE not defined. This should be defined to the warning macro which is to be disabled before including disable_warning.hpp.
    #endif
#endif

#if(!BOOST_PP_IS_EMPTY(STK_DISABLE))
    #if defined(BOOST_MSVC)
        #pragma warning(disable: STK_DISABLE)
    #elif defined(BOOST_COMP_GNUC) && !defined(BOOST_COMP_CLANG)
        #pragma GCC diagnostic ignored STK_DISABLE
    #elif defined(BOOST_COMP_CLANG)
        #if __has_warning(STK_DISABLE)
            #pragma clang diagnostic ignored STK_DISABLE
        #endif
    #else

    #endif//! disable STK_DISABLE
#endif//! If there is a warning.

#undef STK_DISABLE
#ifdef STK_WARNING
    #undef STK_WARNING
#endif

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

#ifndef STK_WARNING
#error STK_WARNING not defined. This should be defined before including disable_warning.hpp.
#endif

#if(!BOOST_PP_IS_EMPTY(STK_WARNING))
    #if defined(BOOST_MSVC)
        #pragma warning(disable: STK_WARNING)
    #elif defined(BOOST_COMP_GNUC) && !defined(BOOST_COMP_CLANG)
        #pragma GCC diagnostic ignored STK_WARNING
    #elif defined(BOOST_COMP_CLANG)
        #if __has_warning(STK_WARNING)
            #pragma clang diagnostic ignored STK_WARNING
        #endif
    #else

    #endif//! disable STK_WARNING
#endif//! If there is a warning.

#undef STK_WARNING

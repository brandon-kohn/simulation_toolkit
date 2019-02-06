#pragma once

#include <boost/config.hpp>

#ifndef STK_NO_CXX14_DEPRECATED_ATTRIBUTE
    #define STK_DEPRECATED [[deprecated]]
    #define STK_DEPRECATED_MSG(msg) [[deprecated(msg)]]
#else
    #if defined(__GNUC__) || defined(__clang__) 

        #define STK_DEPRECATED __attribute__((deprecated))
        #define STK_DEPRECATED_MSG(msg) __attribute__((deprecated(msg)))

    #elif defined(BOOST_MSVC)

        #define STK_DEPRECATED __declspec(deprecated)
        #define STK_DEPRECATED_MSG(msg) __declspec(deprecated(msg))

    #else

        #define STK_DEPRECATED
        #define STK_DEPRECATED_MSG(msg)

    #endif
#endif


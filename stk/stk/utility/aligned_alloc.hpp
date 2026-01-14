//
//! Copyright © 2019
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <cstdlib>

#ifdef STK_NO_CXX17_STD_ALIGNED_ALLOC
	#include <boost/config.hpp>
	#if defined(BOOST_MSVC)
		#include <malloc.h>
	#else
		#include <mm_malloc.h>
	#endif

    namespace stk {
        inline void* aligned_alloc(std::size_t size, std::size_t align)
        {
            return _mm_malloc(size, align);
        }

        inline void (free)(void *p)
        {
            _mm_free(p);
        }
    }//! namespace stk;

#else //! C++17 aligned_alloc exists.. use it.
    namespace stk {
        inline void* aligned_alloc(std::size_t size, std::size_t align)
        {
            return std::aligned_alloc(size, align);
        }

        inline void free(void *p)
        {
            std::free(p);
        }
    }//! namespace stk;
#endif


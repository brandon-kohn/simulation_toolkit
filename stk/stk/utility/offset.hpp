//
//! Copyright © 2014
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <cmath>
#include <type_traits>

namespace stk{
        template <typename T, typename Member, typename std::enable_if<std::is_standard_layout<T>::value, int>::type = 0>
        inline std::size_t offset_of(Member T::* mbr)
        {        
            const T* p = nullptr;
            return reinterpret_cast<const char*>(&(p->*mbr)) - reinterpret_cast<const char*>(p);
        }

        template <typename T, typename Member, typename std::enable_if<std::is_standard_layout<T>::value, int>::type = 0>
        inline std::size_t offset_of(const T* p, Member T::* mbr)
        {        
            return std::abs(reinterpret_cast<const char*>(&(p->*mbr)) - reinterpret_cast<const char*>(p));
        }

#define STK_OFFSET_OF(T, Member) offset_of(&T::Member) 

}//! namespace stk;


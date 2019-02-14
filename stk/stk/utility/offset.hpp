//
//! Copyright © 2019
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <cmath>
#include <type_traits>
#include <boost/config.hpp>

namespace stk{
    
    template <typename T, typename Member>
    BOOST_CONSTEXPR inline std::size_t offset_of(Member T::* mbr)
    {
        static_assert(std::is_standard_layout<T>::value, "T must be standard layout for offset_of to produce correct results. You may use offset_of_unsafe if you are certain the results will be correct.");
        return (const char*)&(((const T*)nullptr)->*mbr) - (const char*)nullptr;
    }

    template <typename T, typename Member>
    BOOST_CONSTEXPR inline std::size_t offset_of(const T* p, Member T::* mbr)
    {
        static_assert(std::is_standard_layout<T>::value, "T must be standard layout for offset_of to produce correct results. You may use offset_of_unsafe if you are certain the results will be correct.");
        return std::abs((const char*)(&(p->*mbr)) - (const char*)p);
    }

    #define STK_OFFSET_OF(T, Member) ::stk::offset_of(&T::Member)

    //! Calculate unsafe offsets. These are considered unsafe because they don't enforce the standard-layout.
    //! In cases of virtual inheritance or other complex inheritance the results are not known to be correct.
    template <typename T, typename Member>
    BOOST_CONSTEXPR inline std::size_t offset_of_unsafe(Member T::* mbr)
    {
        return (const char*)&(((const T*)nullptr)->*mbr) - (const char*)nullptr;
    }

    template <typename T, typename Member>
    BOOST_CONSTEXPR inline std::size_t offset_of_unsafe(const T* p, Member T::* mbr)
    {
        return std::abs((const char*)(&(p->*mbr)) - (const char*)p);
    }

    #define STK_OFFSET_OF_UNSAFE(T, Member) ::stk::offset_of_unsafe((const T*)nullptr, &T::Member)

}//! namespace stk;


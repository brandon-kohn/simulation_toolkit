#pragma once

#include <boost/config.hpp>
#include <cstdint>

#ifndef STK_CACHE_LINE_SIZE
    #define STK_CACHE_LINE_SIZE 64
#endif//! STK_CACHE_LINE_SIZE

#define STK_CACHE_LINE_PAD(size) (size / STK_CACHE_LINE_SIZE) * STK_CACHE_LINE_SIZE + ((size % STK_CACHE_LINE_SIZE) > 0) * STK_CACHE_LINE_SIZE - size

namespace stk { namespace thread {
    namespace detail {
        template<typename T, typename EnableIf=void>
        struct padded_impl
        {
            struct pad
            {
                template <typename...Ts>
                pad(Ts&&... a)
                    : obj(std::forward<Ts>(a)...)
                {}
                BOOST_ALIGNMENT(STK_CACHE_LINE_SIZE)T obj;
                char padding[STK_CACHE_LINE_PAD(sizeof(T))];

                T& operator *()
                {
                    return obj;
                }

                T const& operator *() const
                {
                    return obj;
                }

                T* operator ->()
                {
                    return &obj;
                }

                T const* operator ->() const
                {
                    return &obj;
                }
            };

            using type = pad;
        };

        template<typename T>
        struct padded_impl<T, typename std::enable_if<sizeof(T) % STK_CACHE_LINE_SIZE == 0>::type>
        {
            struct pad
            {
                template <typename...Ts>
                pad(Ts&&... a)
                    : obj(std::forward<Ts>(a)...)
                {}

                BOOST_ALIGNMENT(STK_CACHE_LINE_SIZE)T obj;
 
                T& operator *()
                {
                    return obj;
                }

                T const& operator *() const
                {
                    return obj;
                }

                T* operator ->()
                {
                    return &obj;
                }

                T const* operator ->() const
                {
                    return &obj;
                }  
            };

            using type = pad;
        };
    }//! namespace detail;

    template <typename T>
    using padded = typename detail::padded_impl<T>::type; 

    template <typename T, typename ...Ts>
    inline padded<T> make_padded(Ts&&...a)
    {
        return padded<T>(std::forward<Ts>(a)...);
    }

}}//! namespace stk::thread;



//
//! Copyright © 2010
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#if !defined(STK_DONT_USE_PREPROCESSED_FILES)
    #include "./preprocessed/unsigned_integer.hpp"
#else
#include <boost/preprocessor/iteration/iterate.hpp>
#include <boost/preprocessor/enum.hpp>
#include <boost/preprocessor/enum_params.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include "operators.hpp"
#include <geometrix/utility/assert.hpp>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <ostream>

#if defined(__WAVE__) && defined(STK_CREATE_PREPROCESSED_FILES)
#pragma wave option(preserve: 2, line: 0, output: "preprocessed/unsigned_integer.hpp")
#endif

namespace stk { 

    #define STK_SIGNED_SEQ    (char)                \
                              (signed char)         \
                              (short)               \
                              (long)                \
                              (int)                 \
                              (long long)           \
                              (float)               \
                              (double)              \
                              (long double)         \
    /***/

    #define STK_UNSIGNED_SEQ  (unsigned char)       \
                              (unsigned short)      \
                              (unsigned long)       \
                              (unsigned int)        \
                              (unsigned long long)  \
                              (bool)                \
    /***/

    namespace detail
    {
        template <typename T, typename Enable = void>
        struct make_unsigned 
        {
            using type = T;
        };

        template <typename T>
        struct make_unsigned<T, typename std::enable_if< std::is_integral<T>::value >::type>
            : std::make_unsigned<T>
        {};
       
        //! A policy selector to choose how to convert numeric types for comparison.
        template <typename T, typename U, typename EnableIf = void>
        struct safe_compare_cast_policy_selector
        {};

        //! A policy to convert using the implicit conversion rules specified by ansi.
        //! This happens when the types are the same, or both are signed, or both are unsigned, or either is floating point.
        struct use_ansi_conversion{};        
        template <typename T, typename U>
        struct safe_compare_cast_policy_selector 
        <
            T
          , U
          , typename std::enable_if
            <
                std::is_same<typename std::decay<T>::type, typename std::decay<U>::type>::value ||
                std::numeric_limits<T>::is_signed == std::numeric_limits<U>::is_signed ||
                std::is_floating_point<T>::value ||
                std::is_floating_point<U>::value
            >::type 
        >
        {
            typedef use_ansi_conversion type;
        };

        //! A policy to convert to an unsigned type whose width is the same as T.
        //! This happens when one type is signed and the other is not and both are integral types, and size of U is less than size of T.
        struct convert_to_unsigned_T{};
        template <typename T, typename U>
        struct safe_compare_cast_policy_selector 
        <
            T
          , U
          , typename std::enable_if
            <
                std::numeric_limits<T>::is_signed != std::numeric_limits<U>::is_signed &&
                std::is_integral<T>::value &&
                std::is_integral<U>::value &&
                sizeof(U) < sizeof(T)
            >::type
        >
        {
            typedef convert_to_unsigned_T type;
        };

        //! A policy to convert to an unsigned type whose width is the same as U.
        //! This happens when one type is signed and the other is not and both are integral types, and size of U is >= size of T.
        struct convert_to_unsigned_U{};
        template <typename T, typename U>
        struct safe_compare_cast_policy_selector
        <
            T
          , U
          , typename std::enable_if
            <
                std::numeric_limits<T>::is_signed != std::numeric_limits<U>::is_signed &&
                std::is_integral<U>::value &&
                std::is_integral<T>::value &&
                sizeof(U) >= sizeof(T)
            >::type
        >
        {
            typedef convert_to_unsigned_U type;
        };
        
        //! Same type or both signed or one is a floating point (let default casting deal with it) - return arg by ref
        template < typename T , typename U , typename Arg >
        inline Arg safe_compare_cast_impl(const Arg& u, use_ansi_conversion)
        {
            return u;
        }

        //! Not the same type, T is wider and is not signed, the other is.
        //! Safe to convert to T.
        template 
        <
            typename T
          , typename U
          , typename Arg
        >
        inline typename make_unsigned<T>::type safe_compare_cast_impl(const Arg& u, convert_to_unsigned_T)
        {
            return static_cast<typename make_unsigned<T>::type>(u);
        }
    
        //! Not the same type, U is wider and is not signed, the other is.
        //! Safe to convert to U.
        template < typename T , typename U , typename Arg >
        inline typename make_unsigned<U>::type safe_compare_cast_impl(const Arg& u, convert_to_unsigned_U)
        {
            return static_cast<typename make_unsigned<U>::type>(u);
        }

        template <typename T, typename U, typename Arg>
        auto safe_compare_cast(Arg a) -> decltype(safe_compare_cast_impl<T,U>(a, typename safe_compare_cast_policy_selector<T, U>::type()))
        {
            return safe_compare_cast_impl<T, U>(a, typename safe_compare_cast_policy_selector<T, U>::type());
        }
    }

    //! A class to represent a range of numbers between 0 and numeric_limits<T>::max() - 1.
    //! The value for max T is reserved to mark an invalid unsigned value (-1).
    template <typename T>
    class unsigned_integer
    {
        template <typename U>
        friend class unsigned_integer;

        static_assert(std::is_unsigned<T>::value, "unsigned_integer<T>: T must be unsigned.");

    public:

        static constexpr T invalid = static_cast<T>(-1);

        unsigned_integer()
            : m_value(invalid)
        {}

        #define STK_UNSIGNED_CTOR(r, data, elem)    \
        explicit unsigned_integer(const elem& n)    \
            : m_value(boost::numeric_cast<T>(n))    \
        {}                                          \
        /***/

        template <typename U>
        unsigned_integer(const unsigned_integer<U>& n)
            : m_value(n.is_valid() ? boost::numeric_cast<T>(n.m_value) : invalid)
        {}

        //! Generate constructors for each fundamental numeric type.
        #if defined(__WAVE__) && defined(STK_CREATE_PREPROCESSED_FILES)
        #pragma wave option(preserve: 1)
        #endif
        BOOST_PP_SEQ_FOR_EACH(STK_UNSIGNED_CTOR, _, STK_UNSIGNED_SEQ)

        #undef STK_UNSIGNED_CTOR

        #define STK_SIGNED_CTOR(r, data, elem)                     \
        explicit unsigned_integer(const elem& n)                   \
            : m_value(n < 0 ? invalid : boost::numeric_cast<T>(n)) \
        {}                                                         \
        /***/

        //! Generate constructors for each fundamental numeric type.
        #if defined(__WAVE__) && defined(STK_CREATE_PREPROCESSED_FILES)
        #pragma wave option(preserve: 1)
        #endif
        BOOST_PP_SEQ_FOR_EACH(STK_SIGNED_CTOR, _, STK_SIGNED_SEQ)

        #undef STK_SIGNED_CTOR

        ~unsigned_integer()
        {}

        bool is_valid() const 
        { 
            return m_value != invalid; 
        }

        bool is_invalid() const
        {
            return m_value == invalid;
        }

        #if defined(__WAVE__) && defined(STK_CREATE_PREPROCESSED_FILES)
        #pragma wave option(preserve: 1)
        #endif
        #define STK_UNSIGNEDINTEGER_ASSIGNOP(r, data, elem)   \
        unsigned_integer& operator = (const elem& n)          \
        {                                                     \
            m_value = boost::numeric_cast<T>(n);              \
            return *this;                                     \
        }                                                     \
        /***/

        //! Generate constructors for each fundamental numeric type.
        #if defined(__WAVE__) && defined(STK_CREATE_PREPROCESSED_FILES)
        #pragma wave option(preserve: 1)
        #endif
        BOOST_PP_SEQ_FOR_EACH(STK_UNSIGNEDINTEGER_ASSIGNOP, _, STK_UNSIGNED_SEQ)

        #undef STK_UNSIGNEDINTEGER_ASSIGNOP

        #define STK_SIGNEDINTEGER_ASSIGNOP(r, data, elem)          \
        unsigned_integer& operator = (const elem& n)               \
        {                                                          \
            m_value = n < 0 ? invalid : boost::numeric_cast<T>(n); \
            return *this;                                          \
        }                                                          \
        /***/

        //! Generate constructors for each fundamental numeric type.
        #if defined(__WAVE__) && defined(STK_CREATE_PREPROCESSED_FILES)
        #pragma wave option(preserve: 1)
        #endif
        BOOST_PP_SEQ_FOR_EACH(STK_SIGNEDINTEGER_ASSIGNOP, _, STK_SIGNED_SEQ)

        #undef STK_SIGNEDINTEGER_ASSIGNOP

        template <typename U>
        unsigned_integer& operator =(const unsigned_integer<U>& n)
        {
            m_value = (n.is_valid() ? boost::numeric_cast<T>(n.m_value) : invalid);
            return *this;
        }

        template <typename U>
        bool operator < ( U rhs ) const
        {
            GEOMETRIX_ASSERT(is_valid());
            if( rhs < 0 )
                return false;

            return detail::safe_compare_cast<T,U>(m_value) < detail::safe_compare_cast<T,U>(rhs);
        }

        template <>
        bool operator < ( const unsigned_integer<T>& rhs ) const
        {
            GEOMETRIX_ASSERT(is_valid() && rhs.is_valid());
            return m_value < rhs.m_value;
        }

        template <typename U>
        bool operator > ( U rhs ) const
        {
            GEOMETRIX_ASSERT(is_valid());

            if( rhs < 0 )
                return true;

            return detail::safe_compare_cast<T,U>(m_value) > detail::safe_compare_cast<T,U>(rhs);
        }

        template <>
        bool operator > ( const unsigned_integer<T>& rhs ) const        
        {
            GEOMETRIX_ASSERT(rhs.is_valid());
            return m_value > rhs.m_value;
        }

        template <typename U>
        bool operator == ( const U& rhs ) const
        {
            if(rhs < 0)
                return rhs == U(-1) && is_invalid();
            
            return detail::safe_compare_cast<T,U>(m_value) == detail::safe_compare_cast<T,U>(rhs);
        }

        template <>
        bool operator == ( const unsigned_integer<T>& rhs ) const
        {            
            return m_value == rhs.m_value;
        }

        bool operator == ( bool rhs ) const
        {   
            GEOMETRIX_ASSERT(is_valid());
            return static_cast<bool>(m_value != 0) == rhs;
        }
        
        template <typename U>
        bool operator != ( const U& rhs ) const
        {
            return !operator==(rhs);
        }

        template <>
        bool operator != ( const unsigned_integer<T>& rhs ) const
        {
            return !operator==(rhs);
        }

        bool operator != ( bool rhs ) const
        {   
            GEOMETRIX_ASSERT(is_valid());
            return static_cast<bool>(m_value != 0) != rhs;
        }

        bool operator !() const
        {
            GEOMETRIX_ASSERT(is_valid());
            return m_value == 0; 
        }

        unsigned_integer<T>& operator ++()
        {
            //! Check for overflow.
            if( is_valid() )
                ++m_value;
            return *this;
        }

        unsigned_integer<T>& operator --()
        {
            //! Check for underflow.
            if( is_valid() )
                --m_value;
            return *this;
        }

        unsigned_integer<T>& operator +=( const unsigned_integer<T>& v )
        {
            return *this += v.m_value;
        }

        template <typename U>
        unsigned_integer<T>& operator +=( U v )
        {
            if( is_valid() )
            {
                T diff = (std::numeric_limits<T>::max)() - m_value;
                if( (v >= 0 && detail::safe_compare_cast<T,U>(v) < detail::safe_compare_cast<T,U>(diff)) || (v < 0 && detail::safe_compare_cast<T,U>(m_value) >= detail::safe_compare_cast<T,U>(v)) )
                    m_value += v;
                else
                    m_value = invalid;
            }
            return *this;
        }

        unsigned_integer<T>& operator -=( const unsigned_integer<T>& v )
        {
            return *this -= v.m_value;
        }

        template <typename U>
        unsigned_integer<T>& operator -=( U v )
        {
            if( is_valid() )
            {                
                T diff = (std::numeric_limits<T>::max)() - m_value;
                if( (v >= 0 && detail::safe_compare_cast<T,U>(m_value) >= detail::safe_compare_cast<T,U>(v)) || (v < 0 && detail::safe_compare_cast<T,U>(-v) <= detail::safe_compare_cast<T,U>(diff)) )
                    m_value -= v;
                else
                    m_value = invalid;
            }

            return *this;
        }

        unsigned_integer<T>& operator *=( const unsigned_integer<T>& v )
        {
            return *this *= v.m_value;
        }

        template <typename U>
        unsigned_integer<T>& operator *=( U v )
        {
            if( is_valid() )
            {
                if( v < 0 )
                    m_value = invalid;
                else if( v == 0 )
                    m_value = 0;
                else if( detail::safe_compare_cast<T,U>(v) < detail::safe_compare_cast<T,U>((std::numeric_limits<T>::max)()/v) )
                    m_value *= v;
                else
                    m_value = invalid;
            }

            return (*this);
        }

        unsigned_integer<T>& operator /=( const unsigned_integer<T>& v )
        {
            return *this /= v.m_value;            
        }

        template <typename U>
        unsigned_integer<T>& operator /=( U v )
        {
            if( is_valid() )
            {
                if( v <= 0 )
                    m_value = invalid;
                else
                    m_value /= v;
            }
            return (*this);
        }

        unsigned_integer<T>& operator %=( const unsigned_integer<T>& v )
        {
            return *this %= v.m_value;
        }

        template <typename U>
        unsigned_integer<T>& operator %=( U v )
        {
            if( is_valid() )
            {
                if( v < 0 )
                    m_value = invalid;
                else
                    m_value %= v;
            }
            return (*this);
        }

        unsigned_integer<T>& operator ^=( const unsigned_integer<T>& v )
        {
            return *this ^= v.m_value;
        }

        template <typename U>
        unsigned_integer<T>& operator ^=( U v )
        {
            m_value ^= T(v);
            return (*this);
        }

        unsigned_integer<T>& operator &=( const unsigned_integer<T>& v )
        {
            return *this &= v.m_value;
        }

        template <typename U>
        unsigned_integer<T>& operator &=( U v )
        {
            m_value &= T(v);
            return (*this);
        }

        unsigned_integer<T>& operator |=( const unsigned_integer<T>& v )
        {
            return *this |= v.m_value;
        }

        template <typename U>
        unsigned_integer<T>& operator |=( U v )
        {
            m_value |= T(v);
            return (*this);
        }

        //! Access the underlying value of type T which can be used in switches etc.
        T get_value() const { return m_value; }
        
        operator T() const { GEOMETRIX_ASSERT(is_valid()); return m_value; }

        explicit operator bool() const 
        {
            GEOMETRIX_ASSERT(is_valid());
            return m_value ? true : false;
        }

        //!Friend operators
        #if defined(__WAVE__) && defined(STK_CREATE_PREPROCESSED_FILES)
        #pragma wave option(preserve: 1)
        #endif
        STK_IMPLEMENT_ORDERED_FIELD_OPERATORS(unsigned_integer<T>, long double);
        STK_IMPLEMENT_ORDERED_FIELD_OPERATORS(unsigned_integer<T>, double);
        STK_IMPLEMENT_ORDERED_FIELD_OPERATORS(unsigned_integer<T>, float);
        STK_IMPLEMENT_ORDERED_FIELD_OPERATORS(unsigned_integer<T>, char);
        STK_IMPLEMENT_ORDERED_FIELD_OPERATORS(unsigned_integer<T>, signed char);
        STK_IMPLEMENT_ORDERED_FIELD_OPERATORS(unsigned_integer<T>, unsigned char);
        STK_IMPLEMENT_ORDERED_FIELD_OPERATORS(unsigned_integer<T>, short);
        STK_IMPLEMENT_ORDERED_FIELD_OPERATORS(unsigned_integer<T>, unsigned short);
        STK_IMPLEMENT_ORDERED_FIELD_OPERATORS(unsigned_integer<T>, int);
        STK_IMPLEMENT_ORDERED_FIELD_OPERATORS(unsigned_integer<T>, unsigned int);
        STK_IMPLEMENT_ORDERED_FIELD_OPERATORS(unsigned_integer<T>, long);
        STK_IMPLEMENT_ORDERED_FIELD_OPERATORS(unsigned_integer<T>, unsigned long);
        STK_IMPLEMENT_ORDERED_FIELD_OPERATORS(unsigned_integer<T>, long long);
        STK_IMPLEMENT_ORDERED_FIELD_OPERATORS(unsigned_integer<T>, unsigned long long);

        STK_IMPLEMENT_ORDERED_FIELD_OPERATORS_SELF(unsigned_integer<T>);
        STK_INCREMENTABLE_OPERATOR(unsigned_integer<T>);
        STK_DECREMENTABLE_OPERATOR(unsigned_integer<T>);

        friend std::ostream& operator<<(std::ostream& os, const stk::unsigned_integer<T>& ui)
        {
            os << ui.get_value();
            return os;
        }

    private:

        //! Hide this...
        unsigned_integer<T> operator -() const
        {
            return unsigned_integer<T>(invalid);
        }

        T m_value;

    };

}//! namespace stk;

namespace std
{
    #if defined(__WAVE__) && defined(STK_CREATE_PREPROCESSED_FILES)
    #pragma wave option(preserve: 1)
    #endif
    #define STK_DEFINE_STD_MATH_FUNCTION(fn)                       \
    template <typename T>                                          \
    stk::unsigned_integer<T> fn(const stk::unsigned_integer<T>& v) \
    {                                                              \
        T vd = (T)v;                                               \
        vd = std:: fn (vd);                                        \
        return stk::unsigned_integer<T>(vd);                       \
    }                                                              \
    /***/

    STK_DEFINE_STD_MATH_FUNCTION(sqrt)
    STK_DEFINE_STD_MATH_FUNCTION(cos)
    STK_DEFINE_STD_MATH_FUNCTION(sin)
    STK_DEFINE_STD_MATH_FUNCTION(tan)
    STK_DEFINE_STD_MATH_FUNCTION(atan)
    STK_DEFINE_STD_MATH_FUNCTION(acos)
    STK_DEFINE_STD_MATH_FUNCTION(asin)
    STK_DEFINE_STD_MATH_FUNCTION(exp)
    STK_DEFINE_STD_MATH_FUNCTION(log10)
    STK_DEFINE_STD_MATH_FUNCTION(log)
    STK_DEFINE_STD_MATH_FUNCTION(ceil)
    STK_DEFINE_STD_MATH_FUNCTION(floor)

    #undef STK_DEFINE_STD_MATH_FUNCTION

    template <typename T>
    class numeric_limits< stk::unsigned_integer<T> > : public numeric_limits<T>
    {
    public:

        static stk::unsigned_integer<T> (min)()
        {
            return stk::unsigned_integer<T>((std::numeric_limits<T>::min)());
        }

        static stk::unsigned_integer<T> (max)()
        {
            return stk::unsigned_integer<T>( (std::numeric_limits<T>::max)()-1 );            
        }
    };
}

#undef STK_UNSIGNED_SEQ
#undef STK_SIGNED_SEQ

#endif// STK_DONT_USE_PREPROCESSED_FILES

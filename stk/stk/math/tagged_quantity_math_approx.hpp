//
//! Copyright © 2022
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/math/math_approx.hpp>
#include <geometrix/utility/tagged_quantity_cmath.hpp>

#define STK_TAGGED_QUANTITY_UNARY_FUNCTION( F )           \
template <typename T>                                     \
template <typename Tag, typename X>                       \
inline                                                    \
geometrix::tagged_quantity                                \
<                                                         \
    geometrix::F ## _op<Tag>                              \
  , decltype(stk::F(std::declval<X>()))                   \
>                                                         \
F(const geometrix::tagged_quantity<Tag, X>& a)            \
{                                                         \
    using namespace geometrix;                            \
    using type = tagged_quantity                          \
    <                                                     \
        F ## _op<Tag>                                     \
      , decltype(stk::F(std::declval<X>()))               \
    >;                                                    \
    return type(stk::F(a.value()));                       \
}                                                         \
/***/

#define STK_TAGGED_QUANTITY_BINARY_FUNCTION( F )          \
template <typename T1, typename T2> struct F ## _op;      \
template                                                  \
<                                                         \
    typename Tag1                                         \
  , typename X                                            \
  , typename Tag2                                         \
  , typename Y                                            \
>                                                         \
inline                                                    \
geometrix::tagged_quantity                                \
<                                                         \
    geometrix::F ## _op<Tag1, Tag2>                       \
  , decltype(stk::F(std::declval<X>(),std::declval<Y>())) \
>                                                         \
F( const geometrix::tagged_quantity<Tag1, X>& lhs         \
 , const geometrix::tagged_quantity<Tag2, Y>& rhs)        \
{                                                         \
    using namespace geometrix;                            \
    using type = tagged_quantity                          \
    <                                                     \
        F ## _op<Tag1, Tag2>                              \
      , decltype(stk::F(std::declval<X>()                 \
        , std::declval<Y>()))                             \
    >;                                                    \
    return type( stk::F(lhs.value(),rhs.value()) );       \
}                                                         \
template                                                  \
<                                                         \
    typename Tag                                          \
  , typename X                                            \
  , typename Y                                            \
>                                                         \
inline                                                    \
geometrix::tagged_quantity                                \
<                                                         \
    geometrix::F ## _op<Tag, Y>                           \
  , decltype(stk::F(std::declval<X>(),std::declval<Y>())) \
>                                                         \
F( const geometrix::tagged_quantity<Tag, X>& lhs          \
 , const Y& rhs)                                          \
{                                                         \
    using namespace geometrix;                            \
    using type = tagged_quantity                          \
    <                                                     \
        F ## _op<Tag, Y>                                  \
      , decltype(stk::F(std::declval<X>()                 \
        , std::declval<Y>()))                             \
    >;                                                    \
    return type(stk::F(lhs.value(),rhs));                 \
}                                                         \
template                                                  \
<                                                         \
    typename X                                            \
  , typename Tag                                          \
  , typename Y                                            \
>                                                         \
inline                                                    \
geometrix::tagged_quantity                                \
<                                                         \
    geometrix::F ## _op<X, Tag>                           \
  , decltype(stk::F(std::declval<X>(),std::declval<Y>())) \
>                                                         \
F( const X& lhs                                           \
 , const geometrix::tagged_quantity<Tag, Y>& rhs)         \
{                                                         \
    using namespace geometrix;                            \
    using type = tagged_quantity                          \
    <                                                     \
        F ## _op<X, Tag>                                  \
      , decltype(stk::F(std::declval<X>()                 \
        , std::declval<Y>()))                             \
    >;                                                    \
    return type(stk::F(lhs,rhs.value()));                 \
}                                                         \
/***/ 

namespace stk{
    STK_TAGGED_QUANTITY_UNARY_FUNCTION(sin);
    STK_TAGGED_QUANTITY_UNARY_FUNCTION(asin);
    STK_TAGGED_QUANTITY_UNARY_FUNCTION(cos);
    STK_TAGGED_QUANTITY_UNARY_FUNCTION(acos);
    STK_TAGGED_QUANTITY_UNARY_FUNCTION(tan);
    STK_TAGGED_QUANTITY_UNARY_FUNCTION(atan);
    STK_TAGGED_QUANTITY_UNARY_FUNCTION(exp);
    STK_TAGGED_QUANTITY_UNARY_FUNCTION(log);
    STK_TAGGED_QUANTITY_UNARY_FUNCTION(log10);
    
    STK_TAGGED_QUANTITY_BINARY_FUNCTION(atan2);
}//! namespace stk;


//
//! Copyright © 2008-2018
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#define STK_LESS_THAN_COMP_OPERATORS(T, U)                                               \
    friend bool operator<=(const T& x, const U& y) { return !static_cast<bool>(x > y); } \
    friend bool operator>=(const T& x, const U& y) { return !static_cast<bool>(x < y); } \
    friend bool operator>(const U& x, const T& y)  { return y < x; }                     \
    friend bool operator<(const U& x, const T& y)  { return y > x; }                     \
    friend bool operator<=(const U& x, const T& y) { return !static_cast<bool>(y < x); } \
    friend bool operator>=(const U& x, const T& y) { return !static_cast<bool>(y > x); } \
/***/

#define STK_LESS_THAN_COMP_SELF_OPERATORS(T)                                             \
    friend bool operator>(const T& x, const T& y)  { return y < x; }                     \
    friend bool operator<=(const T& x, const T& y) { return !static_cast<bool>(y < x); } \
    friend bool operator>=(const T& x, const T& y) { return !static_cast<bool>(x < y); } \
/***/

#define STK_EQUALITY_COMP_OPERATORS(T, U)                                                \
    friend bool operator==(const U& y, const T& x) { return x == y; }                    \
    friend bool operator!=(const U& y, const T& x) { return !static_cast<bool>(x == y); }\
    friend bool operator!=(const T& y, const U& x) { return !static_cast<bool>(y == x); }\
/***/

#define STK_EQUALITY_COMP_SELF_OPERATORS(T)                                              \
    friend bool operator!=(const T& x, const T& y) { return !static_cast<bool>(x == y); }\
/***/

#define STK_BINARY_OPERATOR_COMMUTATIVE(T, U, OP)                                        \
    friend T operator OP(T lhs, const U& rhs ) { return lhs OP##= rhs; }                 \
    friend T operator OP( const U& lhs, T rhs ) { return rhs OP##= lhs; }                \
/***/    

#define STK_BINARY_OPERATOR_COMMUTATIVE_SELF(T, OP )                                     \
    friend T operator OP(T lhs, const T& rhs ) { return lhs OP##= rhs; }                 \
/***/

#define STK_BINARY_OPERATOR_NON_COMMUTATIVE(T, U, OP )                                   \
    friend T operator OP(T lhs, const U& rhs ) { return lhs OP##= rhs; }                 \
    friend T operator OP( const U& lhs, const T& rhs ) { return T( lhs ) OP##= rhs; }    \
/***/

#define STK_BINARY_OPERATOR_NON_COMMUTATIVE_SELF(T, OP )                                 \
    friend T operator OP(T lhs, const T& rhs ) { return lhs OP##= rhs; }                 \
/***/

#define STK_IMPLEMENT_TOTALLY_ORDERED_OPERATORS(T, U)                                    \
    STK_LESS_THAN_COMP_OPERATORS(T, U)                                                   \
    STK_EQUALITY_COMP_OPERATORS(T, U)                                                    \
/***/

#define STK_IMPLEMENT_TOTALLY_ORDERED_OPERATORS_SELF(T)                                  \
    STK_LESS_THAN_COMP_SELF_OPERATORS(T)                                                 \
    STK_EQUALITY_COMP_SELF_OPERATORS(T)                                                  \
/***/

#define STK_IMPLEMENT_ORDERED_FIELD_OPERATORS(T, U)                                      \
    STK_IMPLEMENT_TOTALLY_ORDERED_OPERATORS(T, U)                                        \
    STK_BINARY_OPERATOR_COMMUTATIVE(T, U, *)                                             \
    STK_BINARY_OPERATOR_COMMUTATIVE(T, U, +)                                             \
    STK_BINARY_OPERATOR_NON_COMMUTATIVE(T, U, -)                                         \
    STK_BINARY_OPERATOR_NON_COMMUTATIVE(T, U, /)                                         \
    STK_BINARY_OPERATOR_NON_COMMUTATIVE(T, U, %)                                         \
    STK_BINARY_OPERATOR_COMMUTATIVE(T, U, ^)                                             \
    STK_BINARY_OPERATOR_COMMUTATIVE(T, U, &)                                             \
    STK_BINARY_OPERATOR_COMMUTATIVE(T, U, |)                                             \
/***/

#define STK_IMPLEMENT_ORDERED_FIELD_OPERATORS_SELF(T)                                    \
    STK_IMPLEMENT_TOTALLY_ORDERED_OPERATORS_SELF(T)                                      \
    STK_BINARY_OPERATOR_COMMUTATIVE_SELF(T, *)                                           \
    STK_BINARY_OPERATOR_COMMUTATIVE_SELF(T, +)                                           \
    STK_BINARY_OPERATOR_NON_COMMUTATIVE_SELF(T, -)                                       \
    STK_BINARY_OPERATOR_NON_COMMUTATIVE_SELF(T, /)                                       \
    STK_BINARY_OPERATOR_NON_COMMUTATIVE_SELF(T, %)                                       \
    STK_BINARY_OPERATOR_COMMUTATIVE_SELF(T, ^)                                           \
    STK_BINARY_OPERATOR_COMMUTATIVE_SELF(T, &)                                           \
    STK_BINARY_OPERATOR_COMMUTATIVE_SELF(T, |)                                           \
/***/

#define STK_INCREMENT_OPERATOR(T)                                                        \
    friend T operator++(T& x, int)                                                       \
    {                                                                                    \
        auto nrvo = T{x};                                                                \
        ++x;                                                                             \
        return nrv;                                                                      \
    }                                                                                    \
/***/

#define STK_DECREMENT_OPERATOR(T)                                                        \
    friend T operator--(T& x, int)                                                       \
    {                                                                                    \
        auto nrvo = T{x};                                                                \
        --x;                                                                             \
        return nrv;                                                                      \
    }                                                                                    \
/***/

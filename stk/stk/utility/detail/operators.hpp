//
//! Copyright © 2010
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

//! Comparison
#define STK_LESS_THAN_COMPARABLE_OPERATORS(T, U)                                         \
    friend bool operator<=(const T& x, const U& y) { return !static_cast<bool>(x > y); } \
    friend bool operator>=(const T& x, const U& y) { return !static_cast<bool>(x < y); } \
    friend bool operator>(const U& x, const T& y)  { return y < x; }                     \
    friend bool operator<(const U& x, const T& y)  { return y > x; }                     \
    friend bool operator<=(const U& x, const T& y) { return !static_cast<bool>(y < x); } \
    friend bool operator>=(const U& x, const T& y) { return !static_cast<bool>(y > x); } \
/***/

#define STK_LESS_THAN_COMPARABLE_SELF_OPERATORS(T)                                       \
    friend bool operator>(const T& x, const T& y)  { return y < x; }                     \
    friend bool operator<=(const T& x, const T& y) { return !static_cast<bool>(y < x); } \
    friend bool operator>=(const T& x, const T& y) { return !static_cast<bool>(x < y); } \
/***/

#if __cplusplus < 202002L
#define STK_EQUALITY_COMPARABLE_OPERATORS(T, U)                                          \
    friend bool operator==(const U& y, const T& x) { return x.operator ==(y); }          \
    friend bool operator!=(const U& y, const T& x) { return !static_cast<bool>(x == y); }\
    friend bool operator!=(const T& y, const U& x) { return !static_cast<bool>(y == x); }\
/***/
#else
#define STK_EQUALITY_COMPARABLE_OPERATORS(T, U)
#endif

#define STK_EQUALITY_COMPARABLE_SELF_OPERATORS(T)                                        \
    friend bool operator!=(const T& x, const T& y) { return !static_cast<bool>(x == y); }\
/***/

//!Arithmetic
#define STK_BINARY_OPERATOR_COMMUTATIVE( T, U, OP )                       \
    friend T operator OP( T lhs, const U& rhs ) { return lhs OP##= rhs; } \
    friend T operator OP( const U& lhs, T rhs ) { return rhs OP##= lhs; } \
/***/    

#define STK_BINARY_OPERATOR_COMMUTATIVE_SELF( T, OP )                     \
    friend T operator OP( T lhs, const T& rhs ) { return lhs OP##= rhs; } \
/***/

#define STK_BINARY_OPERATOR_NON_COMMUTATIVE( T, U, OP )                   \
    friend T operator OP( T lhs, const U& rhs ) { return lhs OP##= rhs; } \
                                                                          \
    friend T operator OP( const U& lhs, const T& rhs )                    \
    { return T( lhs ) OP##= rhs; }                                        \
/***/

#define STK_BINARY_OPERATOR_NON_COMMUTATIVE_SELF( T, OP )                 \
    friend T operator OP( T lhs, const T& rhs ) { return lhs OP##= rhs; } \
/***/

#define STK_INCREMENTABLE_OPERATOR(T)                                     \
    friend T operator++(T& x, int)                                        \
    {                                                                     \
        T nrv(x);                                                         \
        ++x;                                                              \
        return nrv;                                                       \
    }                                                                     \
/***/

#define STK_DECREMENTABLE_OPERATOR(T)                                     \
    friend T operator--(T& x, int)                                        \
    {                                                                     \
        T nrv(x);                                                         \
        --x;                                                              \
        return nrv;                                                       \
    }                                                                     \
/***/

#define STK_IMPLEMENT_TOTALLY_ORDERED_OPERATORS(T, U)                  \
    STK_LESS_THAN_COMPARABLE_OPERATORS(T, U)                           \
    STK_EQUALITY_COMPARABLE_OPERATORS(T, U)                            \
/***/

#define STK_IMPLEMENT_TOTALLY_ORDERED_OPERATORS_SELF(T)                \
    STK_LESS_THAN_COMPARABLE_SELF_OPERATORS(T)                         \
    STK_EQUALITY_COMPARABLE_SELF_OPERATORS(T)                          \
/***/

#define STK_IMPLEMENT_ORDERED_FIELD_OPERATORS(T, U)                    \
    STK_IMPLEMENT_TOTALLY_ORDERED_OPERATORS(T, U)                      \
    STK_BINARY_OPERATOR_COMMUTATIVE( T, U, * )                         \
    STK_BINARY_OPERATOR_COMMUTATIVE( T, U, + )                         \
    STK_BINARY_OPERATOR_NON_COMMUTATIVE( T, U, - )                     \
    STK_BINARY_OPERATOR_NON_COMMUTATIVE(T, U, / )                      \
    STK_BINARY_OPERATOR_NON_COMMUTATIVE( T, U, % )                     \
    STK_BINARY_OPERATOR_COMMUTATIVE( T, U, ^ )                         \
    STK_BINARY_OPERATOR_COMMUTATIVE( T, U, & )                         \
    STK_BINARY_OPERATOR_COMMUTATIVE( T, U, | )                         \
/***/

#define STK_IMPLEMENT_ORDERED_FIELD_OPERATORS_SELF(T)                  \
    STK_IMPLEMENT_TOTALLY_ORDERED_OPERATORS_SELF(T)                    \
    STK_BINARY_OPERATOR_COMMUTATIVE_SELF( T, * )                       \
    STK_BINARY_OPERATOR_COMMUTATIVE_SELF( T, + )                       \
    STK_BINARY_OPERATOR_NON_COMMUTATIVE_SELF( T, - )                   \
    STK_BINARY_OPERATOR_NON_COMMUTATIVE_SELF( T, / )                   \
    STK_BINARY_OPERATOR_NON_COMMUTATIVE_SELF( T, % )                   \
    STK_BINARY_OPERATOR_COMMUTATIVE_SELF( T, ^ )                       \
    STK_BINARY_OPERATOR_COMMUTATIVE_SELF( T, & )                       \
    STK_BINARY_OPERATOR_COMMUTATIVE_SELF( T, | )                       \
/***/


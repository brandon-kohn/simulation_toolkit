//
//! Copyright © 2019
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/config.hpp>

#if defined(BOOST_MSVC)
    #define STK_WARNING_PUSH() warning(push) 
    #define STK_WARNING_POP() warning(pop) 
#elif defined(BOOST_COMP_GNUC) && !defined(BOOST_COMP_CLANG)
    #define STK_WARNING_PUSH() GCC diagnostic push
    #define STK_WARNING_POP() GCC diagnostic pop
#elif defined(BOOST_COMP_CLANG)
    #define STK_WARNING_PUSH() clang diagnostic push
    #define STK_WARNING_POP() clang diagnostic pop
#else
    #define STK_WARNING_PUSH() 
    #define STK_WARNING_POP() 
#endif//! STK_WARNING_PUSH()

//! The following macro should follow an #include directive to invoke a disabled warning after the warning has been defined into STK_WARNING.
//! For example
//! /code
//! #pragma STK_WARNING_PUSH()
//! #define STK_WARNING STK_WARNING_SIGN_UNSIGNED_COMPARE
//! #include STK_DO_DISABLE_WARNING()
//! ... <code which emits warning> ...
//! #pragma STK_WARNING_POP()
#define STK_DO_DISABLE_WARNING() <stk/compiler/disable_warning.hpp>

//! Sign/unsigned comparison
#if defined(BOOST_MSVC)
    #define STK_WARNING_SIGN_UNSIGNED_COMPARE 4388
#elif defined(BOOST_COMP_GNUC) && !defined(BOOST_COMP_CLANG)
    #define STK_WARNING_SIGN_UNSIGNED_COMPARE "-Wsign-compare"
#elif defined(BOOST_COMP_CLANG)
    #define STK_WARNING_SIGN_UNSIGNED_COMPARE "-Wsign-compare"
#else
    #define STK_WARNING_SIGN_UNSIGNED_COMPARE 
#endif//! STK_WARNING_SIGN_UNSIGNED_COMPARE


//! Warning C4003 not enough arguments for function-like macro invocation
#if defined(BOOST_MSVC)
    #define STK_WARNING_NOT_ENOUGH_ARGS_FOR_MACRO_INVOKE 4003
#else
    #define STK_WARNING_NOT_ENOUGH_ARGS_FOR_MACRO_INVOKE 
#endif//! STK_WARNING_NOT_ENOUGH_ARGS_FOR_MACRO_INVOKE

//!Warning C4706 assignment within conditional expression
#if defined(BOOST_MSVC)
    #define STK_WARNING_ASSIGNMENT_WITHIN_CONDITIONAL_EXPR 4706
#else
    #define STK_WARNING_ASSIGNMENT_WITHIN_CONDITIONAL_EXPR 
#endif//! STK_WARNING_ASSIGNMENT_WITHIN_CONDITIONAL_EXPR


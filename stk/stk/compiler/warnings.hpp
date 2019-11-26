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
    #define STK_WARNING_PUSH() __pragma(warning(push))
    #define STK_WARNING_POP() __pragma(warning(pop))
#elif defined(__GNUC__) && !defined(__clang__)
    #define STK_WARNING_PUSH() _Pragma("GCC diagnostic push")
    #define STK_WARNING_POP() _Pragma("GCC diagnostic pop")
#elif defined(__clang__)
    #define STK_WARNING_PUSH() _Pragma("clang diagnostic push")
    #define STK_WARNING_POP() _Pragma("clang diagnostic pop")
#else
    #define STK_WARNING_PUSH() 
    #define STK_WARNING_POP() 
#endif//! STK_WARNING_PUSH()

//! The following macro should follow an #include directive to invoke a disabled warning after the warning has been defined into STK_WARNING.
//! For example
//! /code
//! #pragma STK_WARNING_PUSH()
//! #define STK_DISABLE STK_WARNING_SIGN_UNSIGNED_COMPARE
//! #include STK_DO_DISABLE_WARNING()
//! ... <code which emits warning> ...
//! #pragma STK_WARNING_POP()
#define STK_DO_DISABLE_WARNING() <stk/compiler/disable_warning.hpp>

//! Sign/unsigned comparison
#if defined(BOOST_MSVC)
    #define STK_WARNING_SIGN_UNSIGNED_COMPARE 4388
#elif defined(__GNUC__) && !defined(__clang__)
    #define STK_WARNING_SIGN_UNSIGNED_COMPARE "-Wsign-compare"
#elif defined(__clang__)
    #define STK_WARNING_SIGN_UNSIGNED_COMPARE "-Wsign-compare"
#else
    #define STK_WARNING_SIGN_UNSIGNED_COMPARE 
#endif//! STK_WARNING_SIGN_UNSIGNED_COMPARE

//! Padding due to alignment.
#if defined(BOOST_MSVC)
    #define STK_WARNING_PADDED 4324
#elif defined(__GNUC__) && !defined(__clang__)
    #define STK_WARNING_PADDED "-Wpadded"
#elif defined(__clang__)
    #define STK_WARNING_PADDED "-Wpadded"
#else
    #define STK_WARNING_PADDED
#endif//! STK_WARNING_PADDED

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

//! Unreachable code
#if defined(BOOST_MSVC)
    #define STK_WARNING_UNREACHABLE_CODE 4702
#elif defined(__GNUC__) && !defined(__clang__)
    #define STK_WARNING_UNREACHABLE_CODE "-Wunreachable-code"
#elif defined(__clang__)
    #define STK_WARNING_UNREACHABLE_CODE "-Wunreachable-code"
#else
    #define STK_WARNING_UNREACHABLE_CODE
#endif//! STK_WARNING_UNREACHABLE_CODE

//! Derived type destructor defined as deleted because base dtor was not accessible/available.
#if defined(BOOST_MSVC)
#define STK_WARNING_DERIVED_DTOR_DELETED_BASE_INACCESSIBLE 4624
#else
#define STK_WARNING_DERIVED_DTOR_DELETED_BASE_INACCESSIBLE 
#endif//! STK_WARNING_DERIVED_DTOR_DELETED_BASE_INACCESSIBLE

//! Unreferenced formal parameter
#if defined(BOOST_MSVC)
    #define STK_WARNING_UNREFERENCED_FORMAL_PARAMETER 4100
#elif defined(__GNUC__) && !defined(__clang__)
    #define STK_WARNING_UNREFERENCED_FORMAL_PARAMETER "-Wunused-parameter"
#elif defined(__clang__)
    #define STK_WARNING_UNREFERENCED_FORMAL_PARAMETER "-Wunused-parameter"
#else
    #define STK_WARNING_UNREFERENCED_FORMAL_PARAMETER
#endif//! STK_WARNING_UNREFERENCED_FORMAL_PARAMETER

//! Unreferenced variable 
#if defined(BOOST_MSVC)
    #define STK_WARNING_UNREFERENCED_VARIABLE 4101
#elif defined(__GNUC__) && !defined(__clang__)
    #define STK_WARNING_UNREFERENCED_VARIABLE "-Wunused-variable"
#elif defined(__clang__)
    #define STK_WARNING_UNREFERENCED_VARIABLE "-Wunused-variable"
#else
    #define STK_WARNING_UNREFERENCED_VARIABLE
#endif//! STK_WARNING_UNREFERENCED_VARIABLE

//! Declaration hides function parameter. 
#if defined(BOOST_MSVC)
    #define STK_WARNING_DECLARATION_HIDES_FUNCTION_PARAMETER 4457
#elif defined(__GNUC__) && !defined(__clang__)
    #define STK_WARNING_DECLARATION_HIDES_FUNCTION_PARAMETER "-Wshadow"
#elif defined(__clang__)
    #define STK_WARNING_DECLARATION_HIDES_FUNCTION_PARAMETER "-Wshadow"
#else
    #define STK_WARNING_DECLARATION_HIDES_FUNCTION_PARAMETER
#endif//! STK_WARNING_DECLARATION_HIDES_FUNCTION_PARAMETER

//! Local declaration hides global declaration. 
#if defined(BOOST_MSVC)
    #define STK_WARNING_LOCAL_DECLARATION_HIDES_GLOBAL 4459
#elif defined(__GNUC__) && !defined(__clang__)
    #define STK_WARNING_LOCAL_DECLARATION_HIDES_GLOBAL "-Wshadow"
#elif defined(__clang__)
    #define STK_WARNING_LOCAL_DECLARATION_HIDES_GLOBAL "-Wshadow"
#else
    #define STK_WARNING_LOCAL_DECLARATION_HIDES_GLOBAL
#endif//! STK_WARNING_LOCAL_DECLARATION_HIDES_GLOBAL

//! Int conversion loses precision.
#if defined(BOOST_MSVC)
    #define STK_WARNING_UNSAFE_INT_CONVERSION 4267
#elif defined(__GNUC__) && !defined(__clang__)
    #define STK_WARNING_UNSAFE_INT_CONVERSION "-Wint-conversion"
#elif defined(__clang__)
    #define STK_WARNING_UNSAFE_INT_CONVERSION "-Wint-conversion"
#else
    #define STK_WARNING_UNSAFE_INT_CONVERSION 
#endif//! STK_WARNING_UNSAFE_INT_CONVERSION

//! Suppress #pragma message(xxx) warnings. NOTE: This is a rather broad stroke. Use wisely.
#if defined(BOOST_MSVC)
    //! Doesn't seem to be a means of doing this on visual studio yet.
    #define STK_WARNING_DISABLE_PRAGMA_MESSAGES 
#elif defined(__GNUC__) && !defined(__clang__)
    #define STK_WARNING_DISABLE_PRAGMA_MESSAGES "-W#pragma-messages"
#elif defined(__clang__)
    #define STK_WARNING_DISABLE_PRAGMA_MESSAGES "-W#pragma-messages"
#else
    #define STK_WARNING_DISABLE_PRAGMA_MESSAGES 
#endif//! STK_WARNING_DISABLE_PRAGMA_MESSAGES

//! Suppress warning from attribute
//
#if defined(BOOST_MSVC)
    //! Doesn't seem to be a means of doing this on visual studio yet.
    #define STK_WARNING_IGNORED_ATTRIBUTES
#elif defined(__GNUC__) && !defined(__clang__)
    #define STK_WARNING_IGNORED_ATTRIBUTES "-Wignored-attributes"
#elif defined(__clang__)
    #define STK_WARNING_IGNORED_ATTRIBUTES "-Wignored-attributes"
#else
    #define STK_WARNING_IGNORED_ATTRIBUTES 
#endif//! STK_WARNING_IGNORED_ATTRIBUTES



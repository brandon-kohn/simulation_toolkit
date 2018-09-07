/*-----------------------------------------------------------------------------+
Copyright (c) 2010-2010: Joachim Faulhaber
+------------------------------------------------------------------------------+
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENCE.txt or copy at
           http://www.boost.org/LICENSE_1_0.txt)
+-----------------------------------------------------------------------------*/
#ifndef STK_ICL_PREDICATES_ELEMENT_EQUAL_HPP_JOFA_101102
#define STK_ICL_PREDICATES_ELEMENT_EQUAL_HPP_JOFA_101102

#include <stk/container/icl/type_traits/predicate.hpp>
#include <stk/container/icl/type_traits/type_to_string.hpp>
namespace stk {namespace icl
{
    template <class Type> 
    struct element_equal : public relation<Type,Type>
    {
        bool operator()(const Type& lhs, const Type& rhs)const
        {
            return is_element_equal(lhs, rhs);
        }
    };

    template<>
    inline std::string unary_template_to_string<icl::element_equal>::apply()  
    { return "="; }

}} // namespace icl boost

#endif


/*-----------------------------------------------------------------------------+
Copyright (c) 2007-2010: Joachim Faulhaber
+------------------------------------------------------------------------------+
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENCE.txt or copy at
           http://www.boost.org/LICENSE_1_0.txt)
+-----------------------------------------------------------------------------*/
#ifndef STK_ICL_DETAIL_STD_SET_HPP_JOFA_101007
#define STK_ICL_DETAIL_STD_SET_HPP_JOFA_101007

#include <set>
#include <boost/config.hpp>
#include <stk/container/icl/type_traits/type_to_string.hpp>
#include <stk/container/icl/type_traits/is_set.hpp>
namespace stk {namespace icl
{

template <class Type>
struct is_set<std::set<Type> >
{ 
    typedef is_set<std::set<Type> > type;
    BOOST_STATIC_CONSTANT(bool, value = true); 
};


template <class Type>
struct type_to_string<std::set<Type> >
{
    static std::string apply()
    { return "set<"+ type_to_string<Type>::apply() +">"; }
};

template <class Type>
struct type_to_string<std::set<Type, std::greater<Type> > >
{
    static std::string apply()
    { return "set<"+ type_to_string<Type>::apply() +" g>"; }
};

}} // namespace icl boost

#endif // STK_ICL_DETAIL_STD_SET_HPP_JOFA_101007


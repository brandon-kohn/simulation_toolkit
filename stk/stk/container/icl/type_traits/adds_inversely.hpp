/*-----------------------------------------------------------------------------+    
Copyright (c) 2010-2010: Joachim Faulhaber
+------------------------------------------------------------------------------+
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENCE.txt or copy at
           http://www.boost.org/LICENSE_1_0.txt)
+-----------------------------------------------------------------------------*/
#ifndef STK_ICL_TYPE_TRAITS_ADDS_INVERSELY_HPP_JOFA_100829
#define STK_ICL_TYPE_TRAITS_ADDS_INVERSELY_HPP_JOFA_100829

#include <stk/container/icl/type_traits/has_inverse.hpp>
#include <stk/container/icl/functors.hpp>
namespace stk { namespace icl
{

template<class Type, class Combiner>
struct adds_inversely
{
    typedef adds_inversely type;
    BOOST_STATIC_CONSTANT(bool, 
        value = (boost::mpl::and_<has_inverse<Type>, is_negative<Combiner> >::value)); 
};

}} // namespace boost icl

#endif // STK_ICL_TYPE_TRAITS_ADDS_INVERSELY_HPP_JOFA_100829



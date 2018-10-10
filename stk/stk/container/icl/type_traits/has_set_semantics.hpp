/*-----------------------------------------------------------------------------+    
Copyright (c) 2008-2009: Joachim Faulhaber
+------------------------------------------------------------------------------+
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENCE.txt or copy at
           http://www.boost.org/LICENSE_1_0.txt)
+-----------------------------------------------------------------------------*/
#ifndef STK_ICL_TYPE_TRAITS_HAS_SET_SEMANTICS_HPP_JOFA_100829
#define STK_ICL_TYPE_TRAITS_HAS_SET_SEMANTICS_HPP_JOFA_100829

#include <boost/mpl/or.hpp>
#include <boost/mpl/and.hpp>
#include <stk/container/icl/type_traits/is_set.hpp>
#include <stk/container/icl/type_traits/is_map.hpp>
#include <stk/container/icl/type_traits/codomain_type_of.hpp>
namespace stk { namespace icl
{
    template <class Type> struct has_set_semantics
    { 
        typedef has_set_semantics<Type> type;
        BOOST_STATIC_CONSTANT(bool, 
            value = (boost::mpl::or_< is_set<Type>
                             , boost::mpl::and_< is_map<Type>
                                        , has_set_semantics
                                          <typename codomain_type_of<Type>::type > 
                                        > 
                             >::value)); 
    };

}} // namespace boost icl

#endif



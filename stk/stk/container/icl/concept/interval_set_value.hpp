/*-----------------------------------------------------------------------------+    
Copyright (c) 2010-2010: Joachim Faulhaber
+------------------------------------------------------------------------------+
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENCE.txt or copy at
           http://www.boost.org/LICENSE_1_0.txt)
+-----------------------------------------------------------------------------*/
#ifndef STK_ICL_CONCEPT_INTERVAL_SET_VALUE_HPP_JOFA_100924
#define STK_ICL_CONCEPT_INTERVAL_SET_VALUE_HPP_JOFA_100924

#include <boost/utility/enable_if.hpp>
#include <stk/container/icl/type_traits/is_interval_container.hpp>
#include <stk/container/icl/concept/interval.hpp>
namespace stk { namespace icl
{

//==============================================================================
//= AlgoUnifiers<Set>
//==============================================================================
template<class Type, class Iterator>
inline typename boost::enable_if<is_interval_set<Type>, typename Type::codomain_type>::type
co_value(Iterator value_)
{ 
    typedef typename Type::codomain_type codomain_type;
    return icl::is_empty(*value_)? codomain_type() : (*value_).lower(); 
}

}} // namespace boost icl

#endif



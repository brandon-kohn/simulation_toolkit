/*-----------------------------------------------------------------------------+    
Copyright (c) 2010-2010: Joachim Faulhaber
+------------------------------------------------------------------------------+
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENCE.txt or copy at
           http://www.boost.org/LICENSE_1_0.txt)
+-----------------------------------------------------------------------------*/
#ifndef STK_ICL_TYPE_TRAITS_IS_ICL_CONTAINER_HPP_JOFA_100831
#define STK_ICL_TYPE_TRAITS_IS_ICL_CONTAINER_HPP_JOFA_100831

#include <boost/mpl/and.hpp> 
#include <boost/mpl/or.hpp> 
#include <boost/mpl/not.hpp> 
#include <stk/container/icl/type_traits/is_element_container.hpp> 
#include <stk/container/icl/type_traits/is_interval_container.hpp> 
#include <stk/container/icl/type_traits/is_set.hpp> 
namespace stk { namespace icl
{
    template <class Type> 
    struct is_icl_container
    { 
        typedef is_icl_container<Type> type;
        BOOST_STATIC_CONSTANT(bool, value = 
            (boost::mpl::or_<  is_element_container<Type>
                     , is_interval_container<Type> >::value));
    };

}} // namespace boost icl

#endif



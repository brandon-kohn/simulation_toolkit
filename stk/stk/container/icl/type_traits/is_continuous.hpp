/*-----------------------------------------------------------------------------+    
Copyright (c) 2008-2009: Joachim Faulhaber
+------------------------------------------------------------------------------+
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENCE.txt or copy at
           http://www.boost.org/LICENSE_1_0.txt)
+-----------------------------------------------------------------------------*/
#ifndef STK_ICL_TYPE_TRAITS_IS_CONTINUOUS_HPP_JOFA_080910
#define STK_ICL_TYPE_TRAITS_IS_CONTINUOUS_HPP_JOFA_080910

#include <string>
#include <boost/mpl/not.hpp>
#include <stk/container/icl/type_traits/is_discrete.hpp>
namespace stk { namespace icl
{
    template <class Type> struct is_continuous
    {
        typedef is_continuous type;
        BOOST_STATIC_CONSTANT(bool, 
            value = boost::mpl::not_<is_discrete<Type> >::value);
    };

}} // namespace boost icl

#endif



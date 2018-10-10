/*-----------------------------------------------------------------------------+    
Copyright (c) 2008-2009: Joachim Faulhaber
+------------------------------------------------------------------------------+
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENCE.txt or copy at
           http://www.boost.org/LICENSE_1_0.txt)
+-----------------------------------------------------------------------------*/
#ifndef STK_ICL_TYPE_TRAITS_IS_DISCRETE_HPP_JOFA_100410
#define STK_ICL_TYPE_TRAITS_IS_DISCRETE_HPP_JOFA_100410

#include <string>
#include <boost/config.hpp> // For macro BOOST_STATIC_CONSTANT
#include <boost/mpl/and.hpp>
#include <boost/mpl/not.hpp>

#ifdef BOOST_MSVC 
#pragma warning(push)
#pragma warning(disable:4913) // user defined binary operator ',' exists but no overload could convert all operands, default built-in binary operator ',' used
#endif                        

#include <boost/detail/is_incrementable.hpp>

#ifdef BOOST_MSVC
#pragma warning(pop)
#endif

#include <boost/type_traits/is_integral.hpp>
#include <boost/type_traits/is_floating_point.hpp>
#include <stk/container/icl/type_traits/rep_type_of.hpp>
#include <stk/container/icl/type_traits/is_numeric.hpp>
namespace stk { namespace icl
{
    template <class Type> struct is_discrete
    {
        typedef is_discrete type;
        BOOST_STATIC_CONSTANT(bool, 
            value = 
                (boost::mpl::and_
                 < 
                     boost::detail::is_incrementable<Type>
                   , boost::mpl::or_
                     < 
                         boost::mpl::and_
                         <
                             boost::mpl::not_<has_rep_type<Type> >
                           , is_non_floating_point<Type>
                         >
                       , boost::mpl::and_
                         <
                             has_rep_type<Type>
                           , is_discrete<typename rep_type_of<Type>::type>
                         >
                     >
                 >::value
                )
            );
    };

}} // namespace stk icl

#endif



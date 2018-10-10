/*-----------------------------------------------------------------------------+
Copyright (c) 2007-2011: Joachim Faulhaber
+------------------------------------------------------------------------------+
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENCE.txt or copy at
           http://www.boost.org/LICENSE_1_0.txt)
+-----------------------------------------------------------------------------*/
#ifndef STK_ICL_SET_HPP_JOFA_070519
#define STK_ICL_SET_HPP_JOFA_070519

#include <stk/container/icl/impl_config.hpp>

#if defined(ICL_USE_BOOST_MOVE_IMPLEMENTATION)
#   include <boost/container/set.hpp>
#elif defined(ICL_USE_STD_IMPLEMENTATION)
#   include <set>
#else 
#   include <set>
#endif

#include <stk/container/icl/associative_element_container.hpp>
#include <stk/container/icl/functors.hpp>

#endif // STK_ICL_SET_HPP_JOFA_070519


/*-----------------------------------------------------------------------------+
Copyright (c) 2010-2010: Joachim Faulhaber
+------------------------------------------------------------------------------+
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENCE.txt or copy at
           http://www.boost.org/LICENSE_1_0.txt)
+-----------------------------------------------------------------------------*/
#ifndef STK_ICL_DETAIL_EXCLUSIVE_LESS_THAN_HPP_JOFA_100929
#define STK_ICL_DETAIL_EXCLUSIVE_LESS_THAN_HPP_JOFA_100929

#include <stk/container/icl/concept/interval.hpp>
namespace stk { namespace icl
{

/// Comparison functor on intervals implementing an overlap free less 
template <class IntervalT>
struct exclusive_less_than 
{
    /** Operator <tt>operator()</tt> implements a strict weak ordering on intervals. */
    bool operator()(const IntervalT& left, const IntervalT& right)const
    { 
        return icl::non_empty::exclusive_less(left, right); 
    }
};

}} // namespace boost icl

#endif



//
//! Copyright © 2008-2018
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/math/operators.hpp>
#include <boost/numeric/interval.hpp>
#include <boost/numeric/interval/compare/tribool.hpp>

namespace stk { namespace math {

template <typename T>
class interval 
{
    using interval_type = boost::numeric::interval<T>;

public:

    interval()
        : m_interval(T{},T{})
    {}

    interval(const T& a, const T& b)
        : m_interval(a,b)
    {}

    explicit interval(const interval_type& o)
        : m_interval(o.lower(), o.upper())
    {}

    #define STK_DECLARE_COMPARE_OPERATOR(R, op)                        \
    R operator op (const T& rhs) const                                 \
    {                                                                  \
        using namespace boost::numeric::interval_lib;                  \
        return compare::tribool::operator op(m_interval, rhs);         \
    }                                                                  \
    R operator op (const interval<T>& rhs) const                       \
    {                                                                  \
        using namespace boost::numeric::interval_lib::compare;         \
        return tribool::operator op(m_interval, rhs.m_interval);       \
    }                                                                  \
    R operator op (const boost::numeric::interval<T>& rhs) const       \
    {                                                                  \
        using namespace boost::numeric::interval_lib;                  \
        return compare::tribool::operator op(m_interval, rhs);         \
    }                                                                  \
    /***/

    STK_DECLARE_COMPARE_OPERATOR(boost::logic::tribool,==);
    STK_DECLARE_COMPARE_OPERATOR(boost::logic::tribool,!=);
    STK_DECLARE_COMPARE_OPERATOR(boost::logic::tribool,<);
    STK_DECLARE_COMPARE_OPERATOR(boost::logic::tribool,>);
    STK_DECLARE_COMPARE_OPERATOR(boost::logic::tribool,<=);
    STK_DECLARE_COMPARE_OPERATOR(boost::logic::tribool,>=);
    //STK_DECLARE_OPERATOR(interval<T>,+);
    //STK_DECLARE_OPERATOR(interval<T>,-);
    //STK_DECLARE_OPERATOR(interval<T>,*);
    //STK_DECLARE_OPERATOR(interval<T>,/);

	bool intersects_interior(const T& v)
	{
		return lower() < v && v < upper();
	}

	bool intersects(const T& v)
	{
		return lower() <= v && v <= upper();
	}

	bool intersects(const interval<T>& v)
	{
		return boost::logic::indeterminate((*this)==v);
	}

	bool intersects(const interval_type& v)
	{
		return boost::logic::indeterminate((*this)==v);
	}

    STK_IMPLEMENT_ORDERED_FIELD_OPERATORS(interval<T>, T);
    STK_IMPLEMENT_ORDERED_FIELD_OPERATORS(interval<T>, interval_type);
    
    T    upper() const { return m_interval.upper(); }
    T    lower() const { return m_interval.lower(); }
    void set(const T& l, const T& u) { m_interval.set(l,u); }
    void set_upper( const T& u ) { m_interval.set(lower(), u); }
    void set_lower( const T& l ) { m_interval.set(l, upper()); }

private:

    interval_type m_interval;
};

BOOST_TRIBOOL_THIRD_STATE(intersects)

}}//namespace stk::math;

#undef STK_DECLARE_COMPARE_OPERATOR

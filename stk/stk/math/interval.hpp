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
#include <boost/numeric/interval/io.hpp>
#include <boost/numeric/interval/compare/certain.hpp>
#include <boost/numeric/interval/compare/possible.hpp>
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
        using namespace boost::numeric::interval_lib::compare;         \
        return possible::operator op(m_interval, rhs);               \
    }                                                                  \
    R operator op (const interval<T>& rhs) const                       \
    {                                                                  \
        using namespace boost::numeric::interval_lib::compare;         \
        return possible::operator op(m_interval, rhs.m_interval);    \
    }                                                                  \
    R operator op (const interval_type& rhs) const                     \
    {                                                                  \
        using namespace boost::numeric::interval_lib::compare;         \
        return possible::operator op(m_interval, rhs);               \
    }                                                                  \
    /***/

    STK_DECLARE_COMPARE_OPERATOR(bool,==);
    STK_DECLARE_COMPARE_OPERATOR(bool,!=);
    STK_DECLARE_COMPARE_OPERATOR(bool,<);
    STK_DECLARE_COMPARE_OPERATOR(bool,>);
    STK_DECLARE_COMPARE_OPERATOR(bool,<=);
    STK_DECLARE_COMPARE_OPERATOR(bool,>=);

    #define STK_DECLARE_BINARY_OPERATOR(op)                            \
    interval<T>& operator op (const T& rhs)                            \
    {                                                                  \
        m_interval op rhs;                                             \
        return *this;                                                  \
    }                                                                  \
    interval<T>& operator op (const interval<T>& rhs)                  \
    {                                                                  \
        m_interval op rhs.m_interval;                                  \
        return *this;                                                  \
    }                                                                  \
    interval<T>& operator op (const interval_type& rhs)                \
    {                                                                  \
        m_interval op rhs;                                             \
        return *this;                                                  \
    }                                                                  \
    /***/

    STK_DECLARE_BINARY_OPERATOR(+=);
    STK_DECLARE_BINARY_OPERATOR(-=);
    STK_DECLARE_BINARY_OPERATOR(*=);
    STK_DECLARE_BINARY_OPERATOR(/=);

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

	template <typename NumberComparisonPolicy>
	bool equivalent(const interval<T>& rhs, const NumberComparisonPolicy& cmp)
	{
		return cmp.equals(lower(), rhs.lower()) && cmp.equals(upper(), rhs.upper());
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

}}//namespace stk::math;

#undef STK_DECLARE_COMPARE_OPERATOR
#undef STK_DECLARE_BINARY_OPERATOR

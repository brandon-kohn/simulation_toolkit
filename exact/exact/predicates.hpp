//
//! Copyright © 2019
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <exact/exact_export.hpp>

#include <stk/geometry/primitive/point.hpp>

#include <geometrix/algorithm/orientation_enum.hpp>

#include <array>
#include <concepts>

namespace exact {

	EXACT_API void init();

	//! Calculate if c is left, right, or collinear wrt the line formed by a->b.
	EXACT_API geometrix::orientation_type orientation(const stk::point2& a, const stk::point2& b, const stk::point2& c);
	EXACT_API geometrix::orientation_type orientation(const std::array<double, 2>& a, const std::array<double, 2>& b, const std::array<double, 2>& c);
	EXACT_API geometrix::orientation_type orientation(const boost::array<double, 2>& a, const boost::array<double, 2>& b, const boost::array<double, 2>& c);
	EXACT_API geometrix::orientation_type orientation(const double* a, const double* b, const double* c);

	//! Check if point d is inside(left), outside(right) or collinear (cocircular) of the circum-circle of a, b, c. Circum-circle is the circle that contains a,b,c on its perimeter.
	//! NOTE: a, b, c must be counterclockwise for the above orientation convention.
	EXACT_API geometrix::orientation_type in_circumcircle(const stk::point2& a, const stk::point2& b, const stk::point2& c, const stk::point2& d);
	EXACT_API geometrix::orientation_type in_circumcircle(const std::array<double, 2>& a, const std::array<double, 2>& b, const std::array<double, 2>& c, const std::array<double, 2>& d);
	EXACT_API geometrix::orientation_type in_circumcircle(const boost::array<double, 2>& a, const boost::array<double, 2>& b, const boost::array<double, 2>& c, const boost::array<double, 2>& d);
	EXACT_API geometrix::orientation_type in_circumcircle(const double* a, const double* b, const double* c, const double* d);

	template <std::floating_point T>
	inline geometrix::orientation_type orientation(const geometrix::point<T, 2>& a, const geometrix::point<T, 2>& b, const geometrix::point<T, 2>& c)
	{
		return orientation( a.sequence(), b.sequence(), c.sequence() );
	}
	
}//! namespace exact;


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

namespace exact {

	EXACT_API void init();

	//! Calculate if c is left, right, or collinear wrt the line formed by a->b.
	EXACT_API geometrix::orientation_type orientation(const stk::point2& a, const stk::point2& b, const stk::point2& c);

	//! Check if point d is inside(left), outside(right) or collinear (cocircular) of the circum-circle of a, b, c. Circum-circle is the circle that contains a,b,c on its perimeter.
	//! NOTE: a, b, c must be counterclockwise for the above orientation convention.
	EXACT_API geometrix::orientation_type in_circumcircle(const stk::point2& a, const stk::point2& b, const stk::point2& c, const stk::point2& d);
	

}//! namespace exact;


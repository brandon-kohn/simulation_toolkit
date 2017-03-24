//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_CURVE_DECOMPOSITION_HPP
#define STK_CURVE_DECOMPOSITION_HPP
#pragma once

#include "geometry_kernel.hpp"
#include <geometrix/algorithm/point_sequence/curvature.hpp>

inline std::vector<polyline2> decompose_polyline_by_curvature(const polyline2& poly, const units::angle& totalCurvature)
{
	using namespace geometrix;

	std::vector<polyline2> results;
	using access = point_sequence_traits<polyline2>;
	polyline2 current{ access::get_point(poly,0) };
	
	auto ci = polyline_signed_curvature_at_index(poly, 0);
	int cSign = sign(ci);
	auto ktot = constants::zero<units::angle>();
	auto k_prev = polyline_total_curvature_over_index(poly, 1) * units::si::radians / point_point_distance( poly[0], poly[1] );;
	units::length segmentDistance = 0.0 * units::si::meters;
	
	auto size = access::size(poly);
	for (std::size_t j = 1; j < size; ++j)
	{
		ktot += polyline_total_curvature_over_index(poly, j) * units::si::radians;
		auto k_new = polyline_total_curvature_over_index(poly, j) * units::si::radians / point_point_distance( poly[j], poly[j-1] );
		auto distance = point_point_distance( poly[j-1], poly[ j ] );
		segmentDistance += distance;

		int nSign = sign(polyline_signed_curvature_at_index(poly, j));
		current.push_back(access::get_point(poly, j));
		if ( segmentDistance > 10.0 * units::si::meters && ( fabs( k_new.value() - k_prev.value() ) > 0.01 || distance.value() > 10.0 )/* || nSign != cSign || ktot > totalCurvature */ )
		{
			if (access::size(current) > 1) {
				results.push_back(std::move(current));
				current.push_back(access::get_point(poly, j));
			}
			cSign = nSign;
			ktot = constants::zero<units::angle>();
			segmentDistance = 0.0 * units::si::meters;
		}
		k_prev = k_new;
	}

	if (access::size(current) > 1)
		results.push_back(std::move(current));

	return results;
}

#endif//STK_CURVE_DECOMPOSITION_HPP

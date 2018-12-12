//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/geometry/rtree_cache.hpp>
#include <stk/geometry/tolerance_policy.hpp>

#include <geometrix/primitive/point_sequence_utilities.hpp>

namespace stk {

template <typename Polygon>
struct polygon_to_indexable
{
    aabb2 operator()(const Polygon& pgon) const 
    {
        auto bounds = geometrix::get_bounds(pgon, make_tolerance_policy());
        return aabb2{point2{ std::get<geometrix::e_xmin>(bounds), std::get<geometrix::e_ymin>(bounds) }, point2{ std::get<geometrix::e_xmax>(bounds), std::get<geometrix::e_ymax>(bounds) } };
    }
};

template <typename Polygon>
struct rtree_polygon_cache 
{
    using cache_t = rtree_cache<Polygon, rtree_cache_traits<polygon_to_indexable>>;
    
    rtree_polygon_cache(const std::vector<Polygon>& pgons);
    cache_t::data_index_set_t find_indices(const point2& p, const units::length& offset = 0.001 * units::si::meters) const;

private:
    
    cache_t mCache;
};

}//! namespace stk;


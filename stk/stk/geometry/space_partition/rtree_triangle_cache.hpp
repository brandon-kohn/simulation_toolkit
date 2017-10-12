//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_RTREE_TRIANGLE_CACHE_HPP
#define STK_RTREE_TRIANGLE_CACHE_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include <stk/geometry/rtree_cache.hpp>
#include <stk/geometry/tolerance_policy.hpp>

#include <geometrix/primitive/point_sequence_utilities.hpp>
#include <geometrix/primitive/array_point_sequence.hpp>

namespace stk {

struct triangle_to_indexable
{
    aabb2 operator()(const std::array<point2, 3>& tri) const 
    {
        auto bounds = geometrix::get_bounds(tri, make_tolerance_policy());
        return aabb2{point2{ std::get<geometrix::e_xmin>(bounds), std::get<geometrix::e_ymin>(bounds) }
        ,   point2{ std::get<geometrix::e_xmax>(bounds), std::get<geometrix::e_ymax>(bounds) } };
    }
};

struct rtree_triangle_cache 
{
    using cache_t = rtree_cache<std::array<point2,3>, rtree_cache_traits<triangle_to_indexable>>;
    
    rtree_triangle_cache(const std::vector<std::array<point2,3>>& trigs);
    
    //! Mesh requires this c'tor for other types of triangle caches.
    rtree_triangle_cache(const std::vector<point2>&, const std::vector<std::array<point2,3>>& trigs)
        : rtree_triangle_cache(trigs)
    {}
    
    cache_t::data_index_set_t find_indices(const point2& p, const units::length& offset = 0.001 * units::si::meters) const;

private:
    
    cache_t mCache;
};

//! geometrix::mesh_2d requires a builder for caches which need additional constructor args.
struct rtree_triangle_cache_builder
{
    using result_type = rtree_triangle_cache;
    result_type operator()(const std::vector<point2>&, const std::vector<std::array<point2, 3>>& triangles) const
    {
        return result_type(triangles);
    }
};

}//! namespace stk;

#endif//STK_RTREE_TRIANGLE_CACHE_HPP

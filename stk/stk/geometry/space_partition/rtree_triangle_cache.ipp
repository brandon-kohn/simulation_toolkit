//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_RTREE_TRIANGLE_CACHE_IPP
#define STK_RTREE_TRIANGLE_CACHE_IPP

#if defined(_MSC_VER)
    #pragma once
#endif

#if BOOST_VERSION >= 107700
#include <boost/geometry/strategies/relate/cartesian.hpp>
#endif
#include <stk/geometry/space_partition/rtree_triangle_cache.hpp>
#include <stk/geometry/rtree_cache.ipp>
#include <stk/geometry/tensor/vector.hpp>

namespace stk {

inline rtree_triangle_cache::rtree_triangle_cache(const std::vector<std::array<point2,3>>& trigs)
: mCache(trigs)
{

}

inline rtree_triangle_cache::cache_t::data_index_set_t rtree_triangle_cache::find_indices(const point2& p, const units::length& offset) const
{
    auto vOffset = vector2{offset, offset};
    auto region = aabb2{ p - vOffset, p + vOffset };
    return mCache.find_indices(region);
}

inline rtree_triangle_cache::cache_t::data_index_set_t rtree_triangle_cache::find_indices(const aabb2& region) const
{
    return mCache.find_indices(region);
}

}//! namespace stk;

#endif//STK_RTREE_TRIANGLE_CACHE_IPP

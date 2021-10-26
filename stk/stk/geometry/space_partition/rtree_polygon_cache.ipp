//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#if BOOST_VERSION >= 107700
#include <boost/geometry/strategies/relate/cartesian.hpp>
#endif
#include <stk/geometry/space_partition/rtree_polygon_cache.hpp>
#include <stk/geometry/rtree_cache.ipp>
#include <stk/geometry/tensor/vector.hpp>

namespace stk {

inline rtree_polygon_cache::rtree_polygon_cache(const std::vector<Polygon>& pgons)
: mCache(pgons)
{

}

inline rtree_polygon_cache::cache_t::data_index_set_t rtree_polygon_cache::find_indices(const point2& p, const units::length& offset) const
{
    auto vOffset = vector2{offset, offset};
    auto region = aabb2{ p - vOffset, p + vOffset };
    return mCache.find_indices(region);
}

}//! namespace stk;


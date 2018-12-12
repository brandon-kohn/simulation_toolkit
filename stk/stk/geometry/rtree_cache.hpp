//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_RTREE_CACHE_HPP
#define STK_RTREE_CACHE_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include <stk/geometry/primitive/point.hpp>
#include <stk/units/boost_units.hpp>
#include <stk/geometry/primitive/axis_aligned_bounding_box.hpp>
#include <boost/optional.hpp>

#include <vector>
#include <type_traits>

namespace stk {

struct rtree_cache_impl;

struct default_get_indexable_policy
{
    template <typename T>
    const T& operator()(const T& d) const
    {
        return d;
    }
};

template <typename GetIndexablePolicy, typename EnableIf=void>
struct rtree_cache_traits
{
    GetIndexablePolicy getIndexable;
};

template <typename Data, typename CacheTraits = rtree_cache_traits<default_get_indexable_policy> >
class rtree_cache
{
public:

    using data_index_set_t = boost::container::flat_set<std::size_t>;
    
    template <typename Inputs>
    rtree_cache(const Inputs& inputs, const CacheTraits& cacheTraits);

    template <typename Inputs>
    rtree_cache(const Inputs& inputs, typename std::enable_if<std::is_default_constructible<CacheTraits>::value>::type* = nullptr)
        : rtree_cache(inputs, CacheTraits())
    {}
    
    template <typename SelectionPolicy>
    boost::optional<Data> find(const point2& p, const SelectionPolicy& selector, const units::length& offset = 0.0001 * units::si::meters) const;

    template <typename SelectionPolicy>
    boost::optional<Data> find(const aabb2& region, const SelectionPolicy& selector) const;

    template <typename Visitor>
    void for_each(const aabb2& region, Visitor&& v) const;

    std::vector<Data> find(const aabb2& region) const;

    data_index_set_t find_indices(const aabb2& region) const;

private:

    CacheTraits                       mCacheTraits;
    std::vector<Data>                 mData;
    std::shared_ptr<rtree_cache_impl> mPImpl;
};

}//! namespace stk;

#endif//STK_RTREE_CACHE_HPP

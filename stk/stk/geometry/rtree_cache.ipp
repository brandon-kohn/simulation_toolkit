//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_RTREE_CACHE_IPP
#define STK_RTREE_CACHE_IPP

#if defined(_MSC_VER)
#pragma once
#endif

#include "rtree_cache.hpp"

#include <boost/container/flat_set.hpp>

#if defined(EI)
#pragma push_macro("EI")
#define STK_SOMEONE_DEFINED_EI
#include <boost/preprocessor/stringize.hpp>
#pragma message("EI = " BOOST_PP_STRINGIZE( EI ) " should not be defined! It interferes with template parameters in boost::geometry::rtree.")
#undef EI
#endif

#include <boost/geometry/index/rtree.hpp>
#if BOOST_VERSION >= 107700
#include <boost/geometry/strategies/relate/cartesian.hpp>
#endif

#if defined(STK_SOMEONE_DEFINED_EI)
#undef STK_SOMEONE_DEFINED_EI
#pragma pop_macro("EI")
#endif

#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/value_type.hpp>
#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>

namespace stk {

	struct rtree_cache_impl
	{
		using point_t = boost::geometry::model::point<double, 2, boost::geometry::cs::cartesian>;
		using box_t = boost::geometry::model::box<point_t>;
		using value_t = std::pair<box_t, std::size_t>;

		static std::size_t const max_capacity = 1024;
		static std::size_t const min_capacity = 340;

		using rtree_t = boost::geometry::index::rtree<value_t, boost::geometry::index::rstar<max_capacity, min_capacity>>;

		template <typename Inputs, typename CacheTraits>
		rtree_cache_impl(const Inputs& inputs, const CacheTraits& cacheTraits)
			: mTree(make_tree(inputs, cacheTraits))
		{}

		template <typename Inputs, typename CacheTraits>
		static rtree_t make_tree(const Inputs& inputs, const CacheTraits& cacheTraits)
		{
			using value_type = typename boost::range_value<Inputs>::type;
			std::size_t count = 0;
			auto toIndex = [&count, &cacheTraits](const value_type& v) -> value_t
			{
				const auto& box = cacheTraits.getIndexable(v);
				return value_t{
					box_t{
					point_t{ box.get_lower_bound()[0].value(), box.get_lower_bound()[1].value() }
					,   point_t{ box.get_upper_bound()[0].value(), box.get_upper_bound()[1].value() }
				}
					, count++
				};
			};

			using boost::adaptors::transformed;
			std::vector<value_t> boxes;
			boxes.reserve(inputs.size());
			boost::copy(inputs | transformed(toIndex), std::back_inserter(boxes));

			return rtree_t{ boxes.begin(), boxes.end() };
		}

		rtree_t mTree;
	};

	template <typename Data, typename CacheTraits>
	template <typename Inputs>
	inline rtree_cache<Data, CacheTraits>::rtree_cache(const Inputs& inputs, const CacheTraits& cacheTraits)
		: mCacheTraits(cacheTraits)
		, mData(inputs)
		, mPImpl(std::make_shared<rtree_cache_impl>(inputs, cacheTraits))
	{}

	template <typename Data, typename CacheTraits /*= rtree_cache_traits<Data> */>
	template <typename SelectionPolicy>
	inline boost::optional<Data> rtree_cache<Data, CacheTraits>::find(const point2& p, const SelectionPolicy& selector, const units::length& offset /*= 0.0001 * units::si::meters*/) const
	{
		vector2 v = { offset, offset };
		return find(aabb2{ p - v, p + v }, selector);
	}
	
    template <typename Data, typename CacheTraits /*= rtree_cache_traits<Data> */>
    template <typename SelectionPolicy>
    inline boost::optional<Data> rtree_cache<Data, CacheTraits>::find(const aabb2& region, const SelectionPolicy& selector) const
    {
		auto results = find_indices(region);

		for (const auto& index : results) 
			if (selector(mData[index])) 
				return mData[index];

		return boost::none;
	}

    template <typename Data, typename CacheTraits /*= rtree_cache_traits<Data> */>
    template <typename Visitor>
    inline void rtree_cache<Data, CacheTraits>::for_each(const aabb2& region, Visitor&& v) const
    {
		auto results = find_indices(region);

		for (const auto& index : results) 
			v(mData[index]);
    }

	template <typename Data, typename CacheTraits>
	inline std::vector<Data> rtree_cache<Data, CacheTraits>::find(const aabb2& r) const
	{
		using boost::adaptors::transformed;

		auto candidates = find_indices(r);

		std::vector<Data> results;
		results.reserve(50);

		auto toData = [&](std::size_t i) { return mData[i]; };
		boost::copy(candidates | transformed(toData), std::back_inserter(results));

		return results;
	}

	template <typename Data, typename CacheTraits>
	inline typename rtree_cache<Data, CacheTraits>::data_index_set_t rtree_cache<Data, CacheTraits>::find_indices(const aabb2& r) const
	{
		using value_t = typename rtree_cache_impl::value_t;
		using box_t = typename rtree_cache_impl::box_t;
		using point_t = typename rtree_cache_impl::point_t;
		using boost::geometry::index::adaptors::queried;
		using boost::adaptors::transformed;
		using boost::geometry::index::intersects;

		data_index_set_t result;
		result.reserve(100);
		auto toIndex = [](const value_t& v) { return v.second; };

		box_t region = { point_t{ r.get_lower_bound()[0].value(), r.get_lower_bound()[1].value() }, point_t{ r.get_upper_bound()[0].value(), r.get_upper_bound()[1].value() } };

		boost::copy(mPImpl->mTree | queried(intersects(region)) | transformed(toIndex), std::inserter(result, result.end()));
		return result;
	}

}//! namespace stk;

#endif//STK_RTREE_CACHE_IPP

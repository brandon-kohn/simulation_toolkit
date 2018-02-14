//
//! Copyright © 2018
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once
 
#include <geometrix/utility/assert.hpp>
#include <boost/range/iterator.hpp>
#include <boost/range/iterator_range.hpp>
#include <functional>
#include <vector>

namespace stk { namespace thread {

	template <typename Container>
	inline std::vector<boost::iterator_range<typename boost::range_iterator<Container>::type>> partition_work(Container& cont, const std::ptrdiff_t num)
	{
		GEOMETRIX_ASSERT(num != 0);
		using iterator = typename boost::range_iterator<Container>::type;
		auto first = std::begin(cont);
		auto last = std::end(cont);
		auto total = std::distance(first, last);
		auto portion = std::ptrdiff_t{ total / num };
		std::vector<boost::iterator_range<iterator>> chunks;
		chunks.reserve(num);
		int remainder = total - (num * portion);
		auto portion_end = first;
		for(auto i = 0;i < num; ++i)
		{
			auto portion_start = portion_end;
			std::advance(portion_end, portion + (--remainder >= 0 ? 1 : 0));
			if(portion_start != portion_end)
				chunks.emplace_back(boost::make_iterator_range(portion_start, portion_end));
		}

		return chunks;
	}

    template <typename Container, typename Fn>
	inline void partition_work(Container& cont, const std::ptrdiff_t num, Fn&& fn)
	{
		GEOMETRIX_ASSERT(num != 0);
		using iterator = typename boost::range_iterator<Container>::type;
		auto first = std::begin(cont);
		auto last = std::end(cont);
		auto total = std::distance(first, last);
		auto portion = std::ptrdiff_t{ total / num };
		int remainder = total - (num * portion);
		auto portion_end = first;
		for(auto i = 0;i < num; ++i)
		{
			auto portion_start = portion_end;
			std::advance(portion_end, portion + (--remainder >= 0 ? 1 : 0));
			if(portion_start != portion_end)
                fn(portion_start, portion_end);
		}
	}

	inline std::vector<std::pair<std::size_t, std::size_t>> partition_work(std::ptrdiff_t nTasks, const std::ptrdiff_t num)
	{
		GEOMETRIX_ASSERT(num != 0);
		auto portion = nTasks / num;
		std::ptrdiff_t remainder = nTasks - (num * portion);
		auto portion_end = 0;
		std::vector<std::pair<std::size_t, std::size_t>> chunks;
		chunks.reserve(num);
		for(auto i = 0;i < num; ++i)
		{
			auto portion_start = portion_end;
			portion_end += portion + (--remainder >= 0 ? 1 : 0);
			if(portion_start != portion_end)
				chunks.emplace_back(portion_start, portion_end);
		}

		return chunks;
	}

    template <typename Fn>
    inline void partition_work(std::ptrdiff_t nTasks, const std::ptrdiff_t num, Fn&& fn)
	{
		GEOMETRIX_ASSERT(num != 0);
		auto portion = nTasks / num;
		std::ptrdiff_t remainder = nTasks - (num * portion);
		auto portion_end = 0;
		for(auto i = 0;i < num; ++i)
		{
			auto portion_start = portion_end;
			portion_end += portion + (--remainder >= 0 ? 1 : 0);
			if(portion_start != portion_end)
                fn(portion_start, portion_end);
		}
	}
}}//! namespace stk::thread;


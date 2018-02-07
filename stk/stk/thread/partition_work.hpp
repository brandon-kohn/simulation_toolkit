//
//! Copyright © 2018
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/range/iterator.hpp>
#include <boost/range/iterator_range.hpp>
#include <functional>
#include <vector>

namespace stk { namespace thread {

	template <typename Container>
	inline std::vector<boost::iterator_range<typename boost::range_iterator<Container>::type>> partition_work(Container& cont, const std::ptrdiff_t num)
	{
		using iterator = typename boost::range_iterator<Container>::type;
		auto first = std::begin(cont);
		auto last = std::end(cont);
		auto total = std::distance(first, last);
		auto portion = std::ptrdiff_t{ total / num };
		std::vector<boost::iterator_range<iterator>> chunks(num);
		int remainder = total - (num * portion);
		auto portion_end = first;
		std::generate(std::begin(chunks), std::end(chunks), [&portion_end, portion, &remainder]()
		{
			auto portion_start = portion_end;
			std::advance(portion_end, portion + (--remainder >= 0 ? 1 : 0));
			return boost::make_iterator_range(portion_start, portion_end);
		});

		return chunks;
	}

	inline std::vector<std::pair<std::size_t, std::size_t>> partition_work(std::ptrdiff_t nTasks, const std::ptrdiff_t num)
	{
		auto portion = nTasks / num;
		std::ptrdiff_t remainder = nTasks - (num * portion);
		auto portion_end = 0;
		std::vector<std::pair<std::size_t, std::size_t>> chunks(num);
		std::generate(std::begin(chunks), std::end(chunks), [&portion_end, portion, &remainder]()
		{
			auto portion_start = portion_end;
			portion_end += portion + (--remainder >= 0 ? 1 : 0);
			return std::make_pair(portion_start, portion_end);
		});

		return chunks;
	}

}}//! namespace stk::thread;


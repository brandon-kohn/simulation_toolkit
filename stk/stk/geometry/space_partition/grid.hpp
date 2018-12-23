//
//! Copyright © 2018
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/units/boost_units.hpp>

#include <geometrix/primitive/segment.hpp>
#include <geometrix/primitive/polyline.hpp>
#include <geometrix/primitive/polygon.hpp>
#include <geometrix/algorithm/grid_traits.hpp>
#include <geometrix/algorithm/hash_grid_2d.hpp>
#include <geometrix/algorithm/fast_voxel_grid_traversal.hpp>

namespace stk {

    template <typename Cell>
    class grid2
    {
        using grid_type = geometrix::hash_grid_2d<Cell, geometrix::grid_traits<stk::units::length>>;

    public:

        grid2(const geometrix::grid_traits<stk::units::length>& g)
            : m_grid(g)
        {}

        template <typename Visitor, typename NumberComparisonPolicy>
        void for_each_cell(const segment2& s, Visitor&& v, const NumberComparisonPolicy& cmp)
        {
            auto fn = [&v, this](std::uint32_t i, std::uint32_t j)
            {
                auto& cell = m_grid.get_cell(i,j);
                v(cell);
            };
	        geometrix::fast_voxel_grid_traversal(m_grid.get_traits(), s, fn, cmp);
        }
        
        template <typename Visitor, typename NumberComparisonPolicy>
        void for_each_cell_on_border(const polygon2& p, Visitor&& v, const NumberComparisonPolicy& cmp)
        {
            auto fn = [&v, this](std::uint32_t i, std::uint32_t j)
            {
                auto& cell = m_grid.get_cell(i,j);
                v(cell);
            };
            for(std::size_t i = 0, j = 1; i < p.size(); ++i, ++j %= p.size())  
	            geometrix::fast_voxel_grid_traversal(m_grid.get_traits(), segment2{p[i], p[j]}, fn, cmp);
        }
        
        template <typename Visitor, typename NumberComparisonPolicy>
        void for_each_cell(const polyline2& p, Visitor&& v, const NumberComparisonPolicy& cmp)
        {
            auto fn = [&v, this](std::uint32_t i, std::uint32_t j)
            {
                auto& cell = m_grid.get_cell(i,j);
                v(cell);
            };
            for(std::size_t i = 0, j = 1; j < p.size(); i = j++)  
	            geometrix::fast_voxel_grid_traversal(m_grid.get_traits(), segment2{p[i], p[j]}, fn, cmp);
        }

    private:

        geometrix::hash_grid_2d<Cell, geometrix::grid_traits<stk::units::length>> m_grid;

    };

}//! namespace stk;


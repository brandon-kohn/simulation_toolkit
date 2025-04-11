//=======================================================================
//! Copyright © 2017 Brandon Kohn
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//=======================================================================
#pragma once

#include "crs_graph.hpp"
#include <vector>
#include <cstdint>
#include <iterator>
#include <utility>
#include <boost/iterator/counting_iterator.hpp>
#include <boost/iterator/iterator_adaptor.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/property_map/property_map.hpp>

namespace stk::graph::boost_adapters {

	using vertex_weight_pair_t = std::pair<vertex_t, weight_t>;
	using vertex_weight_pair_vector = std::vector<vertex_weight_pair_t, aligned_allocator<vertex_weight_pair_t>>;

	struct crs_graph_adapter
	{
		using vertex_descriptor = vertex_t;

		struct edge_descriptor
		{
			vertex_t source;
			vertex_t target;
			weight_t weight;
			auto     operator<=>( const edge_descriptor& ) const = default;
		};

		const crs_graph& g;
		alignas( std::hardware_destructive_interference_size ) vertex_weight_pair_vector data; // cache-friendly alignment

		explicit crs_graph_adapter( const crs_graph& graph )
			: g( graph )
		{
			data.reserve( graph.targets.size() );
			for( std::size_t i = 0, e = graph.targets.size(); i < e; ++i )
				data.emplace_back( graph.targets[i], graph.weights[i] );
		}
	};

	class crs_out_edge_iterator
		: public boost::iterator_adaptor<
			  crs_out_edge_iterator,
			  vertex_weight_pair_vector::const_iterator,
			  crs_graph_adapter::edge_descriptor,
			  std::forward_iterator_tag,
			  crs_graph_adapter::edge_descriptor>
	{
	public:
		crs_out_edge_iterator() noexcept
			: m_source( 0 )
		{}

		crs_out_edge_iterator(
			vertex_weight_pair_vector::const_iterator it,
			vertex_t                                          source ) noexcept
			: crs_out_edge_iterator::iterator_adaptor_( it )
			, m_source( source )
		{}

	private:
		friend class boost::iterator_core_access;

		crs_graph_adapter::edge_descriptor dereference() const noexcept
		{
			const auto& [target, weight] = *this->base_reference();
			return { m_source, target, weight };
		}

		vertex_t m_source;
	};

	inline std::pair<boost::counting_iterator<vertex_t>,
		boost::counting_iterator<vertex_t>>
	vertices( const crs_graph_adapter& adapter )
	{
		vertex_t n = static_cast<vertex_t>( adapter.g.row_starts.size() - 1 );
		return { boost::counting_iterator<vertex_t>( 0 ),
			boost::counting_iterator<vertex_t>( n ) };
	}

	inline std::size_t num_vertices( const crs_graph_adapter& adapter ) noexcept
	{
		return adapter.g.row_starts.size() - 1;
	}

	inline std::pair<crs_out_edge_iterator, crs_out_edge_iterator>
	out_edges( vertex_t v, const crs_graph_adapter& adapter )
	{
		const std::size_t start = adapter.g.row_starts[v];
		const std::size_t end = adapter.g.row_starts[v + 1];
		return { crs_out_edge_iterator( adapter.data.begin() + start, v ),
			crs_out_edge_iterator( adapter.data.begin() + end, v ) };
	}

	inline std::size_t out_degree( vertex_t v, const crs_graph_adapter& adapter ) noexcept
	{
		return adapter.g.row_starts[v + 1] - adapter.g.row_starts[v];
	}

	inline vertex_t source( const crs_graph_adapter::edge_descriptor& e,
		const crs_graph_adapter& ) noexcept
	{
		return e.source;
	}

	inline vertex_t target( const crs_graph_adapter::edge_descriptor& e,
		const crs_graph_adapter& ) noexcept
	{
		return e.target;
	}

	struct crs_edge_weight_map
	{
		using key_type = crs_graph_adapter::edge_descriptor;
		using value_type = weight_t;
		using reference = const weight_t&;
		using category = boost::readable_property_map_tag;

		inline const weight_t& operator[]( const key_type& k ) const noexcept
		{
			return k.weight;
		}
	};

	inline const weight_t& get( const crs_edge_weight_map&, const crs_graph_adapter::edge_descriptor& k ) noexcept
	{
		return k.weight;
	}

} // namespace stk::graph::boost_adapters

namespace boost {

	template <>
	struct property_map<stk::graph::boost_adapters::crs_graph_adapter, edge_weight_t>
	{
		using type = stk::graph::boost_adapters::crs_edge_weight_map;
		using const_type = stk::graph::boost_adapters::crs_edge_weight_map;
	};

	inline stk::graph::boost_adapters::crs_edge_weight_map
	get( edge_weight_t, const stk::graph::boost_adapters::crs_graph_adapter& ) noexcept
	{
		return {};
	}

	template <>
	struct graph_traits<stk::graph::boost_adapters::crs_graph_adapter>
	{
		using vertex_descriptor = stk::graph::boost_adapters::crs_graph_adapter::vertex_descriptor;
		using edge_descriptor = stk::graph::boost_adapters::crs_graph_adapter::edge_descriptor;
		using directed_category = directed_tag;
		using edge_parallel_category = disallow_parallel_edge_tag;
		using vertices_size_type = std::size_t;
		using edges_size_type = std::size_t;
		using vertex_iterator = std::pair<boost::counting_iterator<stk::graph::vertex_t>,
			boost::counting_iterator<stk::graph::vertex_t>>;
		using out_edge_iterator = stk::graph::boost_adapters::crs_out_edge_iterator;
		using degree_size_type = std::size_t;
		using traversal_category = incidence_graph_tag;
	};

} // namespace boost

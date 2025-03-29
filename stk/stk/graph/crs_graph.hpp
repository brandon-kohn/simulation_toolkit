//!
//! @file crs_graph.hpp
//! @brief compressed row storage graph.

#pragma once

#include <vector>
#include <utility>
#include <cstddef>
#include <cstdint>
#include <cassert>
#include <limits>
#include <algorithm>
#include <functional>

#include <queue>
#include <boost/heap/d_ary_heap.hpp>
#include <boost/heap/fibonacci_heap.hpp>

namespace stk::graph {

	//! Use 32-bits for each type for cache locality.
	using vertex_t = std::uint32_t;
	using weight_t = float;

	struct crs_graph
	{
		weight_t get_edge_weight( vertex_t u, vertex_t v ) const
		{
			std::size_t begin = row_starts[u];
			std::size_t end = row_starts[u + 1];

			for( std::size_t i = begin; i < end; ++i )
			{
				if( targets[i] == v )
				{
					return weights[i];
				}
			}

			return std::numeric_limits<weight_t>::infinity(); // No edge found
		}

		std::vector<vertex_t>                targets;    //! target vertex indices
		std::vector<weight_t>                weights;    //! edge weights
		std::vector<std::size_t>             row_starts; //! row start offsets per vertex
		std::vector<std::pair<float, float>> positions;  //! 2D positions per vertex
	};

	//! Default priority queue policy using std::priority_queue
	struct default_priority_queue_policy
	{
		template <typename T, typename Compare>
		using type = std::priority_queue<T, std::vector<T>, Compare>;
	};

	//! Boost d-ary heap policy (e.g., binary, 4-ary, etc.)
	template <unsigned Arity = 4>
	struct d_ary_heap_policy
	{
		template <typename T, typename Compare>
		using type = boost::heap::d_ary_heap<T, boost::heap::arity<Arity>, boost::heap::compare<Compare>>;
	};

	//! Boost Fibonacci heap policy
	struct fibonacci_heap_policy
	{
		template <typename T, typename Compare>
		using type = boost::heap::fibonacci_heap<T, boost::heap::compare<Compare>>;
	};

	//! Customizable priority queue policy wrapper for Dijkstra/A*
	template <typename T, typename Compare, typename Policy = default_priority_queue_policy>
	using priority_queue_t = typename Policy::template type<T, Compare>;

	//! Helper alias for (f, node) style entries
	template <typename Policy = default_priority_queue_policy>
	using weight_queue_t = priority_queue_t<std::pair<weight_t, vertex_t>, std::greater<>, Policy>;

	class crs_graph_builder
	{
	public:
		crs_graph_builder( std::size_t num_vertices, bool undirected = false )
			: m_num_vertices( num_vertices )
			, m_undirected( undirected )
			, m_temp_adj( num_vertices )
			, m_positions( num_vertices, { 0.0f, 0.0f } )
		{}

		void set_position( vertex_t v, float x, float y )
		{
			m_positions[v] = { x, y };
		}

		void add_edge( vertex_t u, vertex_t v, weight_t weight )
		{
			m_temp_adj[u].emplace_back( v, weight );
			if( m_undirected && u != v )
				m_temp_adj[v].emplace_back( u, weight );
		}

		crs_graph build()
		{
			std::vector<std::size_t> row_starts( m_num_vertices + 1, 0 );
			for( std::size_t u = 0; u < m_num_vertices; ++u )
				row_starts[u + 1] = row_starts[u] + m_temp_adj[u].size();

			std::vector<vertex_t> targets;
			std::vector<weight_t> weights;
			targets.reserve( row_starts.back() );
			weights.reserve( row_starts.back() );

			for( const auto& list : m_temp_adj )
			{
				for( const auto& [v, w] : list )
				{
					targets.push_back( v );
					weights.push_back( w );
				}
			}

			return crs_graph{
				std::move( targets ),
				std::move( weights ),
				std::move( row_starts ),
				std::move( m_positions )
			};
		}

	private:
		std::size_t                                             m_num_vertices;
		bool                                                    m_undirected;
		std::vector<std::vector<std::pair<vertex_t, weight_t>>> m_temp_adj;
		std::vector<std::pair<float, float>>                    m_positions;
	};

} // namespace stk::graph

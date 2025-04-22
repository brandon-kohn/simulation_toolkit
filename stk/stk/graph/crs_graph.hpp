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
#include <tuple>
#include <type_traits>

#include <stk/utility/aligned_allocator.hpp>

#include <boost/graph/adjacency_list.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/graph/graph_traits.hpp>
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

			return std::numeric_limits<weight_t>::infinity(); //! No edge found
		}
		std::vector<vertex_t, aligned_allocator<vertex_t>> targets;
		std::vector<weight_t, aligned_allocator<weight_t>> weights;
		std::vector<vertex_t, aligned_allocator<vertex_t>> row_starts;
		std::vector<std::pair<float, float>>               positions; //! 2D positions per vertex
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
			std::vector<vertex_t, stk::aligned_allocator<vertex_t>> row_starts( m_num_vertices + 1, 0 );
			for( std::size_t u = 0; u < m_num_vertices; ++u )
				row_starts[u + 1] = row_starts[u] + m_temp_adj[u].size();

			std::vector<vertex_t, stk::aligned_allocator<vertex_t>> targets;
			std::vector<weight_t, stk::aligned_allocator<weight_t>> weights;
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

	//! The boost_crs_builder converts a given Boost graph directly into a CRS graph.
	//! It is templated on the BoostGraph type.
	class boost_crs_builder
	{
	public:
		template <typename BoostGraph>
		static crs_graph build( const BoostGraph& bg )
		{
			//! Type aliases for convenience.
			using vertex_descriptor = typename boost::graph_traits<BoostGraph>::vertex_descriptor;
			using edge_descriptor = typename boost::graph_traits<BoostGraph>::edge_descriptor;
			using weight_t = float; //! Change this if your weight type differs.
			crs_graph cg;
			//! Get number of vertices.
			const size_t n = boost::num_vertices( bg );
			cg.row_starts.clear();
			cg.row_starts.resize( n + 1, 0 );

			//! First pass: count the edges (outdegree) for each vertex.
			size_t index = 0;
			for( auto v : boost::make_iterator_range( boost::vertices( bg ) ) )
			{
				//! The out_edges range.
				auto   edges = boost::out_edges( v, bg );
				size_t deg = std::distance( edges.first, edges.second );
				cg.row_starts[index + 1] = cg.row_starts[index] + deg;
				++index;
			}

			const size_t total_edges = cg.row_starts[n];
			cg.targets.clear();
			cg.targets.resize( total_edges );
			cg.weights.clear();
			cg.weights.resize( total_edges );

			//! Second pass: Fill in the targets and weights arrays.
			index = 0;
			for( auto v : boost::make_iterator_range( boost::vertices( bg ) ) )
			{
				const size_t start = cg.row_starts[index];
				size_t       pos = start;
				for( auto e : boost::make_iterator_range( boost::out_edges( v, bg ) ) )
				{
					//! Map the target vertex to a size_t index.
					vertex_descriptor tgt = boost::target( e, bg );
					cg.targets[pos] = static_cast<size_t>( tgt );
					//! Extract the weight from the edge property.
					cg.weights[pos] = bg[e].weight;
					++pos;
				}
				++index;
			}

			return cg;
		}
	};

} // namespace stk::graph

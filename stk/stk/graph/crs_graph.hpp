//!
//!  @file crs_graph.hpp
//!  @brief compressed row storage graph.

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

	//!  Use 32-bits for each type for cache locality.
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
		std::vector<std::pair<float, float>>               positions; //!  2D positions per vertex
	};

	//!  Default priority queue policy using std::priority_queue
	struct default_priority_queue_policy
	{
		template <typename T, typename Compare>
		using type = std::priority_queue<T, std::vector<T>, Compare>;
	};

	//!  Boost d-ary heap policy (e.g., binary, 4-ary, etc.)
	template <unsigned Arity = 4>
	struct d_ary_heap_policy
	{
		template <typename T, typename Compare>
		using type = boost::heap::d_ary_heap<T, boost::heap::arity<Arity>, boost::heap::compare<Compare>>;
	};

	//!  Boost Fibonacci heap policy
	struct fibonacci_heap_policy
	{
		template <typename T, typename Compare>
		using type = boost::heap::fibonacci_heap<T, boost::heap::compare<Compare>>;
	};

	//!  Customizable priority queue policy wrapper for Dijkstra/A*
	template <typename T, typename Compare, typename Policy = default_priority_queue_policy>
	using priority_queue_t = typename Policy::template type<T, Compare>;

	//!  Helper alias for (f, node) style entries
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

	namespace detail {
		//! Helper trait to check for a member named m_value.
		template <typename T, typename = void>
		struct has_m_value : std::false_type
		{};

		template <typename T>
		struct has_m_value<T, std::void_t<decltype( std::declval<T>().m_value )>> : std::true_type
		{};
	} // namespace detail

	//! This function builds masks for vertices and edges from a given Boost graph.
	//! VertexPred is a callable taking a vertex (of type vertex_descriptor) and returning bool.
	//! EdgePred is a callable taking an edge (of type edge_descriptor) and returning bool.
	template <typename BoostGraph, typename VertexPred, typename EdgePred>
	std::tuple<std::vector<std::uint8_t>, std::vector<std::uint8_t>>
	build_masks( const BoostGraph& bg, VertexPred vertex_pred, EdgePred edge_pred )
	{
		using vertex_descriptor = typename boost::graph_traits<BoostGraph>::vertex_descriptor;
		using edge_descriptor = typename boost::graph_traits<BoostGraph>::edge_descriptor;

		//! Build vertex mask.
		const std::size_t         numVertices = boost::num_vertices( bg );
		std::vector<std::uint8_t> vertex_mask( numVertices, 0 );
		for( auto v : boost::make_iterator_range( boost::vertices( bg ) ) )
		{
			//! For graphs with vecS, the vertex descriptor is typically a size_t.
			if( vertex_pred( v ) )
				vertex_mask[v] = 1;
		}

		//! Build edge mask.
		const std::size_t         numEdges = boost::num_edges( bg );
		std::vector<std::uint8_t> edge_mask( numEdges, 0 );
		for( auto e : boost::make_iterator_range( boost::edges( bg ) ) )
		{
			if( edge_pred( e ) )
			{
				size_t idx = 0;
				if constexpr( detail::has_m_value<edge_descriptor>::value )
					idx = e.m_value; //! Use the faster m_value if available.
				else
					idx = boost::get( boost::edge_index, bg, e ); //! Fallback using property map.
				edge_mask[idx] = 1;
			}
		}

		return std::make_tuple( vertex_mask, edge_mask );
	}

} // namespace stk::graph

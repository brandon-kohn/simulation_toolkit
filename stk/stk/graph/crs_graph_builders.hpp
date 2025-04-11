#pragma once

#include "crs_graph.hpp"
#include <vector>
#include <cstdint>
#include <utility>
#include <algorithm>

// We include the Boost internal header that defines histogram_sort.
// (Note: This is an internal header and its interface may change between Boost releases.)
#include <boost/graph/detail/histogram_sort.hpp>

namespace stk::graph {

	// A sorted builder that uses Boost’s histogram_sort to order the edges.
	class crs_graph_builder_histogram_sorted
	{
	public:
		crs_graph_builder_histogram_sorted( vertex_t num_vertices, bool undirected = false )
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
			// Count total edges
			size_t total_edges = 0;
			for( vertex_t u = 0; u < m_num_vertices; ++u )
			{
				total_edges += m_temp_adj[u].size();
			}

			// Accumulate all edges into global unsorted vectors.
			std::vector<vertex_t> unsorted_sources;
			std::vector<vertex_t> unsorted_targets;
			std::vector<weight_t> unsorted_weights;
			unsorted_sources.reserve( total_edges );
			unsorted_targets.reserve( total_edges );
			unsorted_weights.reserve( total_edges );

			for( vertex_t u = 0; u < m_num_vertices; ++u )
			{
				for( const auto& edge : m_temp_adj[u] )
				{
					unsorted_sources.push_back( u );
					unsorted_targets.push_back( edge.first );
					unsorted_weights.push_back( edge.second );
				}
			}

			// Allocate output arrays:
			// row_starts will have m_num_vertices+1 elements.
			// targets and weights will each have total_edges elements.
			std::vector<vertex_t, stk::aligned_allocator<vertex_t>> row_starts( m_num_vertices + 1, 0 );
			std::vector<vertex_t, stk::aligned_allocator<vertex_t>> targets( total_edges );
			std::vector<weight_t, stk::aligned_allocator<weight_t>> weights( total_edges );

			auto pred = []( auto ) { return true; };
			auto xform = boost::typed_identity_property_map<vertex_t>();
			boost::graph::detail::count_starts( 
				  unsorted_sources.begin()
				, unsorted_sources.end()
				, row_starts.begin()
				, m_num_vertices
				, pred
				, boost::make_property_map_function( xform ) 
			);

			// Use Boost's histogram_sort to sort the unsorted edge list by source.kkk
			boost::graph::detail::histogram_sort(
				unsorted_sources.begin()
				, unsorted_sources.end()
				, row_starts.begin()
				, m_num_vertices
				, unsorted_targets.begin()
				, targets.begin()
				, unsorted_weights.begin()
				, weights.begin()
				, pred
				, boost::make_property_map_function( xform ) 
			);

			return crs_graph{
				std::move( targets ),
				std::move( weights ),
				std::move( row_starts ),
				std::move( m_positions )
			};
		}

	private:
		vertex_t m_num_vertices;
		bool     m_undirected;
		// Temporary adjacency: one vector per vertex holding (target, weight) pairs.
		std::vector<std::vector<std::pair<vertex_t, weight_t>>> m_temp_adj;
		std::vector<std::pair<float, float>>                    m_positions;
	};

} // namespace stk::graph

//! Copyright © 2017
//! Brandon Kohn
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "stk/graph/crs_graph.hpp"
#include "stk/graph/crs_graph_builders.hpp"
#include "stk/graph/dijkstras_shortest_path.hpp"
#include "stk/graph/boost_adapters.hpp"
#include "stk/graph/temporary_vertex_graph_adaptor.hpp"
#include <stk/utility/compressed_integer_pair.hpp>

#include <boost/graph/compressed_sparse_row_graph.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/iterator/counting_iterator.hpp>
#include <boost/iterator/iterator_adaptor.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>
#include <cstdint>
#include <utility>
#include <ranges>
#include <algorithm>

// For vertex filtering.
// (Not used in the Boost adapter test, but left here for conformity.)
template <typename Vertex>
struct VertexFilter
{
	const std::vector<uint8_t>& vertex_mask;
	VertexFilter( const std::vector<uint8_t>& mask )
		: vertex_mask( mask )
	{}
	bool operator()( const Vertex& v ) const
	{
		return vertex_mask[v] != 0; // true if vertex is "active"
	}
};

// For edge filtering.
template <typename Edge>
struct EdgeFilter
{
	const std::vector<uint8_t>& edge_mask;
	EdgeFilter( const std::vector<uint8_t>& mask )
		: edge_mask( mask )
	{}
	bool operator()( const Edge& e ) const
	{
		// For Boost CSR graphs using vecS for edge storage,
		// e.m_value is an implementation detail available when using vecS.
		// Adjust if needed.
		return edge_mask[e.m_value] != 0;
	}
};

namespace {
	// Abstract vertex property: just an ID.
	struct VertexProperties
	{
		int id;
		VertexProperties( int id = 0 )
			: id( id )
		{}
	};

	// Abstract edge property: a weight.
	struct EdgeProperties
	{
		stk::graph::weight_t weight;
		EdgeProperties( stk::graph::weight_t weight = 0.0f )
			: weight( weight )
		{}
	};

	// Define a Boost CSR graph type with directedS (no backward structure).
	using BoostCSRGraph = boost::compressed_sparse_row_graph<
		boost::directedS,
		VertexProperties,
		EdgeProperties,
		boost::no_property,
		stk::graph::vertex_t,
		stk::graph::vertex_t>;
	using CSRVertex = BoostCSRGraph::vertex_descriptor;
	using CSREdge = BoostCSRGraph::edge_descriptor;
	using CSREdgeProperty = BoostCSRGraph::edge_bundled;

	// Define a Boost adjacency_list graph type.
	using BoostAdjListGraph = boost::adjacency_list<
		boost::vecS,
		boost::vecS,
		boost::directedS,
		VertexProperties,
		EdgeProperties>;
	using AdjVertex = boost::graph_traits<BoostAdjListGraph>::vertex_descriptor;
	using AdjEdge = boost::graph_traits<BoostAdjListGraph>::edge_descriptor;
} // namespace

TEST( DSPPerformanceTest, CompareBoostCSRAdjListAndCRS )
{
#ifdef NDEBUG
	constexpr int                  iterations = 100; // Number of iterations for timing.
#else
	constexpr int                  iterations = 1; // Number of iterations for timing.
#endif
	constexpr int                  gridRows = 1000;  // Grid rows.
	constexpr int                  gridCols = 500;   // Grid columns.
	constexpr stk::graph::vertex_t numVertices = gridRows * gridCols;
	constexpr int                  edgesPerVertex = 100; // Configurable number of edges per vertex.

	// Candidate neighbor offsets in the grid (row offset, column offset).
	// Order: right, down, down-right, down-left.
	const std::vector<std::pair<int, int>> candidateOffsets = {
		{ 0, 1 },
		{ 1, 0 },
		{ 1, 1 },
		{ 1, -1 }
	};

	//========================
	// Build the global edge list and weight list for all three graphs.
	//========================
	std::vector<std::pair<stk::graph::vertex_t, stk::graph::vertex_t>> edge_list;
	std::vector<EdgeProperties>                                        edge_weights;
	edge_list.reserve( numVertices * edgesPerVertex );
	edge_weights.reserve( numVertices * edgesPerVertex );

	for( int i = 0; i < gridRows; ++i )
	{
		for( int j = 0; j < gridCols; ++j )
		{
			int idx = i * gridCols + j;
			int edgesAdded = 0;
			for( const auto& offset : candidateOffsets )
			{
				if( edgesAdded >= edgesPerVertex )
					break;
				int ni = i + offset.first;
				int nj = j + offset.second;
				if( ni >= 0 && ni < gridRows && nj >= 0 && nj < gridCols )
				{
					int neighborIdx = ni * gridCols + nj;
					// Weight based on candidate order.
					float weight = 1.0f + edgesAdded * 0.1f;
					edge_list.push_back( { static_cast<stk::graph::vertex_t>( idx ),
						static_cast<stk::graph::vertex_t>( neighborIdx ) } );
					edge_weights.emplace_back( weight );
					++edgesAdded;
				}
			}
		}
	}

	//========================
	// Build the Boost CSR Graph.
	//========================
	BoostCSRGraph boostCSRGraph( boost::edges_are_unsorted_multi_pass,
		edge_list.begin(),
		edge_list.end(),
		edge_weights.begin(),
		numVertices );

	//========================
	// Build the Boost Adjacency List Graph.
	//========================
	BoostAdjListGraph boostAdjGraph( edge_list.begin(),
		edge_list.end(),
		edge_weights.begin(),
		numVertices );

	//========================
	// Build the CRS Graph using your sorted builder.
	//========================
	using stk::graph::crs_graph_builder_histogram_sorted;
	using stk::graph::crs_graph;
	auto builder = std::make_unique<crs_graph_builder_histogram_sorted>( numVertices, false );
	for( int idx = 0; idx < numVertices; ++idx )
		builder->set_position( idx, 0.0f, 0.0f );
	for( int i = 0; i < gridRows; ++i )
	{
		for( int j = 0; j < gridCols; ++j )
		{
			int idx = i * gridCols + j;
			int edgesAdded = 0;
			for( const auto& offset : candidateOffsets )
			{
				if( edgesAdded >= edgesPerVertex )
					break;
				int ni = i + offset.first;
				int nj = j + offset.second;
				if( ni >= 0 && ni < gridRows && nj >= 0 && nj < gridCols )
				{
					int   neighborIdx = ni * gridCols + nj;
					float weight = 1.0f + edgesAdded * 0.1f;
					builder->add_edge( idx, neighborIdx, weight );
					++edgesAdded;
				}
			}
		}
	}
	crs_graph crsGraph = builder->build();

	//========================
	// Wrap our crs_graph with the Boost adapter.
	//========================
	stk::graph::boost_adapters::crs_graph_adapter adapter( crsGraph );

	//========================
	// Test Boost Dijkstra on our adapter.
	//========================
	uint64_t                          crs_adp_total_time = 0;
	std::vector<stk::graph::weight_t> crs_adp_Dist_final;
	std::vector<stk::graph::vertex_t> crs_adp_Preds_final;
	for( int iter = 0; iter < iterations; ++iter )
	{
		auto                                                     start = std::chrono::high_resolution_clock::now();
		boost::typed_identity_property_map<stk::graph::vertex_t> index;
		constexpr auto                                           inf = std::numeric_limits<stk::graph::weight_t>::max();
		auto                                                     vrange = std::views::iota( 0U, numVertices );
		std::vector<CSRVertex>                                   preds( vrange.begin(), vrange.end() );
		std::vector<stk::graph::weight_t>                        costs( numVertices, inf );
		auto                                                     predecessor_map = boost::make_iterator_property_map( preds.begin(), index );
		auto                                                     distance_map = boost::make_iterator_property_map( costs.begin(), index );
		std::vector<boost::default_color_type>                   colors( numVertices, boost::white_color );
		auto                                                     color_map = boost::make_iterator_property_map( colors.begin(), index );

		boost::put( distance_map, 0, 0.0f );
		boost::dijkstra_shortest_paths_no_init(
			adapter,
			0,
			predecessor_map,
			distance_map,
			boost::get( boost::edge_weight, adapter ),
			index,
			std::less<>(),
			boost::closed_plus<stk::graph::weight_t>(),
			stk::graph::weight_t{},
			boost::default_dijkstra_visitor{},
			color_map );
		auto end = std::chrono::high_resolution_clock::now();
		crs_adp_total_time += std::chrono::duration_cast<std::chrono::milliseconds>( end - start ).count();
		if( iter == iterations - 1 )
		{
			crs_adp_Dist_final = costs;
			crs_adp_Preds_final = preds;
		}
	}
	uint64_t csr_adp_avg_time = crs_adp_total_time / iterations;
	std::clog << "Boost Dijkstra (Adapted CRS) average time: " << csr_adp_avg_time << " ms\n";

	//========================
	// Time CRS Graph Dijkstra (Custom DSP) - with masks.
	//========================
	uint64_t                          crs_total_time = 0;
	std::vector<stk::graph::weight_t> crsDist_final;
	std::vector<stk::graph::vertex_t> crsPreds_final;
	// Create masks (all valid)
	std::vector<uint8_t> vertex_mask( numVertices, 1 );
	std::vector<uint8_t> edge_mask( crsGraph.targets.size(), 1 );
	for( int iter = 0; iter < iterations; ++iter )
	{
		auto start = std::chrono::high_resolution_clock::now();
		auto result = stk::graph::dijkstra<stk::graph::d_ary_heap_policy<>>( crsGraph, 0, vertex_mask, edge_mask );
		auto end = std::chrono::high_resolution_clock::now();
		crs_total_time += std::chrono::duration_cast<std::chrono::milliseconds>( end - start ).count();
		if( iter == iterations - 1 )
		{
			crsDist_final = result.first;
			crsPreds_final = result.second;
		}
	}
	uint64_t crs_avg_time = crs_total_time / iterations;
	std::clog << "CRS Dijkstra on abstract graph (" << gridRows << "x" << gridCols << ", " << numVertices
			  << " vertices) average time: " << crs_avg_time << " ms\n";

	//========================
	// Time Boost Dijkstra on Boost CSR Graph.
	//========================
	uint64_t                          boost_csr_total_time = 0;
	std::vector<stk::graph::weight_t> boostCSRCosts_final;
	std::vector<CSRVertex>            boostCSRPreds_final;
	for( int iter = 0; iter < iterations; ++iter )
	{
		auto                                   start = std::chrono::high_resolution_clock::now();
		constexpr auto                         inf = std::numeric_limits<stk::graph::weight_t>::max();
		auto                                   vrange = std::views::iota( 0U, numVertices );
		std::vector<CSRVertex>                 preds( vrange.begin(), vrange.end() );
		std::vector<stk::graph::weight_t>      costs( numVertices, inf );
		auto                                   index = boost::get( boost::vertex_index, boostCSRGraph );
		auto                                   predecessor_map = boost::make_iterator_property_map( preds.begin(), index );
		auto                                   distance_map = boost::make_iterator_property_map( costs.begin(), index );
		std::vector<boost::default_color_type> colors( numVertices, boost::white_color );
		auto                                   color_map = boost::make_iterator_property_map( colors.begin(), index );
		boost::put( distance_map, 0, 0.0f );
		boost::dijkstra_shortest_paths_no_init(
			boostCSRGraph,
			0,
			predecessor_map,
			distance_map,
			boost::get( &EdgeProperties::weight, boostCSRGraph ),
			index,
			std::less<>(),
			boost::closed_plus<stk::graph::weight_t>(),
			stk::graph::weight_t{},
			boost::default_dijkstra_visitor{},
			color_map );
		auto end = std::chrono::high_resolution_clock::now();
		boost_csr_total_time += std::chrono::duration_cast<std::chrono::milliseconds>( end - start ).count();
		if( iter == iterations - 1 )
		{
			boostCSRCosts_final = costs;
			boostCSRPreds_final = preds;
		}
	}
	uint64_t boost_csr_avg_time = boost_csr_total_time / iterations;
	std::clog << "Boost Dijkstra (CSR) average time: " << boost_csr_avg_time << " ms\n";

	//========================
	// Time Boost Dijkstra on Boost Adjacency List Graph.
	//========================
	uint64_t                          boost_adj_total_time = 0;
	std::vector<stk::graph::weight_t> boostAdjCosts_final;
	std::vector<AdjVertex>            boostAdjPreds_final;
	for( int iter = 0; iter < iterations; ++iter )
	{
		auto                                   start = std::chrono::high_resolution_clock::now();
		constexpr auto                         inf = std::numeric_limits<stk::graph::weight_t>::max();
		auto                                   vrange = std::views::iota( 0U, numVertices );
		std::vector<AdjVertex>                 preds( vrange.begin(), vrange.end() );
		std::vector<stk::graph::weight_t>      costs( numVertices, inf );
		auto                                   index = boost::get( boost::vertex_index, boostAdjGraph );
		auto                                   predecessor_map = boost::make_iterator_property_map( preds.begin(), index );
		auto                                   distance_map = boost::make_iterator_property_map( costs.begin(), index );
		std::vector<boost::default_color_type> colors( numVertices, boost::white_color );
		auto                                   color_map = boost::make_iterator_property_map( colors.begin(), index );
		boost::put( distance_map, 0, 0.0f );
		boost::dijkstra_shortest_paths_no_init(
			boostAdjGraph,
			0,
			predecessor_map,
			distance_map,
			boost::get( &EdgeProperties::weight, boostAdjGraph ),
			index,
			std::less<>(),
			boost::closed_plus<stk::graph::weight_t>(),
			stk::graph::weight_t{},
			boost::default_dijkstra_visitor{},
			color_map );
		auto end = std::chrono::high_resolution_clock::now();
		boost_adj_total_time += std::chrono::duration_cast<std::chrono::milliseconds>( end - start ).count();
		if( iter == iterations - 1 )
		{
			boostAdjCosts_final = costs;
			boostAdjPreds_final = preds;
		}
	}
	uint64_t boost_adj_avg_time = boost_adj_total_time / iterations;
	std::clog << "Boost Dijkstra (Adjacency List) average time: " << boost_adj_avg_time << " ms\n";

	//========================
	// Sanity Check: Print distances to the last vertex.
	//========================
	int lastIndex = numVertices - 1;
	std::clog << "Distance (Boost CSR) to vertex " << lastIndex << ": " << boostCSRCosts_final[lastIndex] << "\n";
	std::clog << "Distance (Boost AdjList) to vertex " << lastIndex << ": " << boostAdjCosts_final[lastIndex] << "\n";
	std::clog << "Distance (Boost Adapter) to vertex " << lastIndex << ": " << crs_adp_Dist_final[lastIndex] << "\n";
	std::clog << "Distance (CRS DSP) to vertex " << lastIndex << ": " << crsDist_final[lastIndex] << "\n";

	bool match = ( boostCSRCosts_final[lastIndex] == boostAdjCosts_final[lastIndex] ) && ( boostCSRCosts_final[lastIndex] == crs_adp_Dist_final[lastIndex] ) && ( boostCSRCosts_final[lastIndex] == crsDist_final[lastIndex] );
	std::clog << "All Dijkstra results match: " << ( match ? "Yes" : "No" ) << "\n";
	std::clog.flush();
}

TEST( TemporaryVertexGraphAdaptorTests, TestStaticAssert )
{
	using namespace stk::graph;

	static_assert( std::is_same_v<edge_property_type_of<BoostCSRGraph>::type, EdgeProperties> );
}

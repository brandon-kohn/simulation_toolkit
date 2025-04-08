//! Copyright © 2017
//! Brandon Kohn
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include "stk/graph/crs_graph.hpp"
#include "stk/graph/dijkstras_shortest_path.hpp"
#include <boost/graph/filtered_graph.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>
#include <cstdint>

// For vertex filtering.
// The predicate returns true for vertices that are enabled.
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
		// For Boost graphs using vecS for edge storage, you can use:
		return edge_mask[e.m_value] != 0; // Adjust this if needed.
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
		float weight;
		EdgeProperties( float weight = 0.0f )
			: weight( weight )
		{}
	};

	// Define a simple undirected graph using Boost.
	using BoostGraph = boost::adjacency_list<
		boost::vecS,
		boost::vecS,
		boost::undirectedS,
		VertexProperties,
		EdgeProperties>;
	using Vertex = BoostGraph::vertex_descriptor;
	using Edge = BoostGraph::edge_descriptor;
} // namespace

// Test focusing solely on DSP using 500,000 vertices.
TEST( DSPPerformanceTest, CompareBoostAndCRS )
{
	constexpr int iterations = 100; // Adjust iterations as desired

	// Grid dimensions: 1000 rows * 500 columns = 500,000 vertices.
	constexpr int gridRows = 1000;
	constexpr int gridCols = 500;
	const int     numVertices = gridRows * gridCols;

	//========================
	// Build the Boost Graph
	//========================
	BoostGraph          boostGraph;
	std::vector<Vertex> boostVertices;
	boostVertices.reserve( numVertices );

	// Create vertices with a simple ID.
	for( int i = 0; i < gridRows; ++i )
	{
		for( int j = 0; j < gridCols; ++j )
		{
			int    idx = i * gridCols + j;
			Vertex v = boost::add_vertex( VertexProperties( idx ), boostGraph );
			boostVertices.push_back( v );
		}
	}

	// Add horizontal and vertical edges.
	// Horizontal edge weight: 1.0f + (j % 10) * 0.1f
	// Vertical edge weight:   1.0f + (i % 10) * 0.1f
	for( int i = 0; i < gridRows; ++i )
	{
		for( int j = 0; j < gridCols; ++j )
		{
			int idx = i * gridCols + j;
			if( j + 1 < gridCols )
			{
				float weight = 1.0f + ( j % 10 ) * 0.1f;
				boost::add_edge( boostVertices[idx], boostVertices[idx + 1], EdgeProperties( weight ), boostGraph );
			}
			if( i + 1 < gridRows )
			{
				float weight = 1.0f + ( i % 10 ) * 0.1f;
				boost::add_edge( boostVertices[idx], boostVertices[idx + gridCols], EdgeProperties( weight ), boostGraph );
			}
		}
	}

	//========================
	// Time Boost Dijkstra
	//========================
	uint64_t            boost_total_time = 0;
	std::vector<float>  boostCosts_final;
	std::vector<Vertex> boostPreds_final;

	for( int iter = 0; iter < iterations; ++iter )
	{
		std::vector<Vertex> boostPreds( num_vertices( boostGraph ) );
		std::vector<float>  boostCosts( boostPreds.size(), std::numeric_limits<float>::max() );

		auto start = std::chrono::high_resolution_clock::now();
		boost::dijkstra_shortest_paths( boostGraph, boostVertices.front(), boost::predecessor_map( &boostPreds[0] ).distance_map( &boostCosts[0] ).weight_map( boost::get( &EdgeProperties::weight, boostGraph ) ).distance_inf( std::numeric_limits<float>::max() ).distance_zero( 0 ) );
		auto end = std::chrono::high_resolution_clock::now();
		boost_total_time += std::chrono::duration_cast<std::chrono::milliseconds>( end - start ).count();

		if( iter == iterations - 1 )
		{
			boostCosts_final = boostCosts;
			boostPreds_final = boostPreds;
		}
	}
	uint64_t boost_avg_time = boost_total_time / iterations;
	std::clog << "Boost Dijkstra on abstract graph ("
			  << gridRows << "x" << gridCols << ", " << numVertices
			  << " vertices) average time: " << boost_avg_time << " ms\n";


	//========================
	// Build the CRS Graph
	//========================
	using stk::graph::crs_graph_builder;
	using stk::graph::crs_graph;
	auto builder = std::make_unique<crs_graph_builder>( numVertices, /*undirected=*/true );
	// Set dummy positions.
	for( int idx = 0; idx < numVertices; ++idx )
		builder->set_position( idx, 0.0f, 0.0f );
	// Add edges with the same weights.
	for( int i = 0; i < gridRows; ++i )
	{
		for( int j = 0; j < gridCols; ++j )
		{
			int idx = i * gridCols + j;
			if( j + 1 < gridCols )
			{
				float weight = 1.0f + ( j % 10 ) * 0.1f;
				builder->add_edge( idx, idx + 1, weight );
			}
			if( i + 1 < gridRows )
			{
				float weight = 1.0f + ( i % 10 ) * 0.1f;
				builder->add_edge( idx, idx + gridCols, weight );
			}
		}
	}
	crs_graph crsGraph = builder->build();

	// Masks (all valid) for the DSP versions that use them.
	std::vector<uint8_t> vertex_mask( numVertices, 1 );
	std::vector<uint8_t> edge_mask( crsGraph.targets.size(), 1 );

	//========================
	// Time CRS Graph Dijkstra (DSP) - with masks
	//========================
	uint64_t                          crs_total_time = 0;
	std::vector<float>                crsDist_final;
	std::vector<stk::graph::vertex_t> crsPreds_final;

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
	std::clog << "CRS Dijkstra on abstract graph ("
			  << gridRows << "x" << gridCols << ", " << numVertices
			  << " vertices) average time: " << crs_avg_time << " ms\n";

	//========================
	// Time CRS Graph Pre-Filtered Dijkstra (DSP) - with masks
	//========================
	uint64_t                          pre_total_time = 0;
	std::vector<float>                precrsDist_final;
	std::vector<stk::graph::vertex_t> precrsPreds_final;

	for( int iter = 0; iter < iterations; ++iter )
	{
		auto start = std::chrono::high_resolution_clock::now();
		auto result = stk::graph::dijkstra_prefilter_thread<stk::graph::d_ary_heap_policy<>>( crsGraph, 0, vertex_mask, edge_mask );
		auto end = std::chrono::high_resolution_clock::now();
		pre_total_time += std::chrono::duration_cast<std::chrono::milliseconds>( end - start ).count();
		if( iter == iterations - 1 )
		{
			precrsDist_final = result.first;
			precrsPreds_final = result.second;
		}
	}
	uint64_t pre_avg_time = pre_total_time / iterations;
	std::clog << "Pre-filtered CRS Dijkstra on abstract graph ("
			  << gridRows << "x" << gridCols << ", " << numVertices
			  << " vertices) average time: " << pre_avg_time << " ms\n";

	//========================
	// Time CRS Graph No-Mask Dijkstra (DSP)
	//========================
	uint64_t                          nomask_total_time = 0;
	std::vector<float>                nomaskDist_final;
	std::vector<stk::graph::vertex_t> nomaskPreds_final;

	for( int iter = 0; iter < iterations; ++iter )
	{
		auto start = std::chrono::high_resolution_clock::now();
		auto result = stk::graph::dijkstra_nomask<stk::graph::d_ary_heap_policy<>>( crsGraph, 0 );
		auto end = std::chrono::high_resolution_clock::now();
		nomask_total_time += std::chrono::duration_cast<std::chrono::milliseconds>( end - start ).count();
		if( iter == iterations - 1 )
		{
			nomaskDist_final = result.first;
			nomaskPreds_final = result.second;
		}
	}
	uint64_t nomask_avg_time = nomask_total_time / iterations;
	std::clog << "CRS Dijkstra (no mask) on abstract graph ("
			  << gridRows << "x" << gridCols << ", " << numVertices
			  << " vertices) average time: " << nomask_avg_time << " ms\n";

	// Optional sanity check: print the computed distance to the last vertex.
	int lastIndex = numVertices - 1;
	std::clog << "Distance (Boost) to vertex " << lastIndex << ": " << boostCosts_final[lastIndex] << "\n";
	std::clog << "Distance (CRS) to vertex " << lastIndex << ": " << crsDist_final[lastIndex] << "\n";
	std::clog << "Distance (pre_CRS) to vertex " << lastIndex << ": " << precrsDist_final[lastIndex] << "\n";
	std::clog << "Distance (no_mask CRS) to vertex " << lastIndex << ": " << nomaskDist_final[lastIndex] << "\n";

	std::clog << "CRS Dijkstra and Boost Dijkstra results match: "
			  << ( boostCosts_final[lastIndex] == crsDist_final[lastIndex] ? "Yes" : "No" ) << "\n";
	std::clog.flush();
}

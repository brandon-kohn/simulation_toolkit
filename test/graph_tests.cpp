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

/*
 #pragma once

#include <vector>
#include <limits>
#include <cstddef>
#include <boost/graph/adjacency_list.hpp>
#include "stk/graph/crs_graph.hpp"
#include "stk/graph/dsp_nomask.hpp"         // Contains dijkstra_nomask
#include "stk/graph/dijkstras_shortest_path.hpp" // Contains d_ary_heap_policy

namespace stk {
namespace graph {

using vertex_t = std::size_t;
using weight_t = float;

//---------------------------------------------------------------------
// Function: build_filtered_crs
//
// Description:
//   Given an unfiltered Boost graph 'bg' and masks (vertex_mask and edge_mask)
//   (each a std::vector<uint8_t> where nonzero means "valid"), this function
//   builds a CRS-format subgraph containing only valid vertices and edges.
//   It also builds two mapping arrays: one mapping original vertex indices
//   to subgraph indices (orig_to_sub) and one mapping subgraph indices back
//   to original vertex indices (sub_to_orig).
//
// Note:
//   Thread-local storage is used to avoid repeated allocations.
//---------------------------------------------------------------------
template <typename BoostGraph>
void build_filtered_crs(const BoostGraph& bg,
                        const std::vector<uint8_t>& vertex_mask,
                        const std::vector<uint8_t>& edge_mask,
                        crs_graph& sub_crs,
                        std::vector<vertex_t>& orig_to_sub,
                        std::vector<vertex_t>& sub_to_orig)
{
    const size_t n = boost::num_vertices(bg);
    // Reuse the caller-provided vectors.
    orig_to_sub.clear();
    orig_to_sub.resize(n, static_cast<vertex_t>(-1));
    sub_to_orig.clear();
    for (size_t v = 0; v < n; ++v) {
        if (vertex_mask[v]) {
            orig_to_sub[v] = sub_to_orig.size();
            sub_to_orig.push_back(v);
        }
    }

    const size_t sub_n = sub_to_orig.size();

    // Build the CRS (Compressed Row Storage) arrays for the subgraph.
    std::vector<size_t> row_starts(sub_n + 1, 0);
    size_t total_edges = 0;
    for (size_t i = 0; i < sub_n; ++i) {
        vertex_t orig = sub_to_orig[i];
        size_t valid_degree = 0;
        auto edges = boost::out_edges(orig, bg);
        for (auto it = edges.first; it != edges.second; ++it) {
            vertex_t tgt = boost::target(*it, bg);
            if (edge_mask[it->m_value] && vertex_mask[tgt])
                ++valid_degree;
        }
        total_edges += valid_degree;
        row_starts[i + 1] = row_starts[i] + valid_degree;
    }

    std::vector<vertex_t> targets(total_edges);
    std::vector<weight_t> weights(total_edges);

    size_t current_edge = 0;
    for (size_t i = 0; i < sub_n; ++i) {
        vertex_t orig = sub_to_orig[i];
        auto edges = boost::out_edges(orig, bg);
        for (auto it = edges.first; it != edges.second; ++it) {
            vertex_t tgt = boost::target(*it, bg);
            if (edge_mask[it->m_value] && vertex_mask[tgt]) {
                // Map the target from the original graph to its subgraph index.
                targets[current_edge] = orig_to_sub[tgt];
                weights[current_edge] = bg[*it].weight;
                ++current_edge;
            }
        }
    }

    sub_crs.row_starts = std::move(row_starts);
    sub_crs.targets    = std::move(targets);
    sub_crs.weights    = std::move(weights);
}

//---------------------------------------------------------------------
// Function: run_dsp_on_subgraph
//
// Description:
//   Given a CRS-format subgraph (with no masks) and a mapping from subgraph 
//   indices back to the original vertex indices (sub_to_orig), this function
//   calls dijkstra_nomask on the subgraph from the specified sub_source vertex.
//   It then creates result vectors (of the same size as the original graph)
//   by mapping the subgraph's distances and predecessor values back to the 
//   original indexing.
//
// Parameters:
//   sub_crs      - The CRS subgraph containing only valid vertices/edges.
//   sub_to_orig  - Mapping from subgraph indices to original graph indices.
//   sub_source   - The source vertex in subgraph coordinates.
//   n            - The number of vertices in the original graph.
// Returns:
//   A pair of vectors (distances, predecessors) sized to 'n' in the original graph.
//---------------------------------------------------------------------
std::pair<std::vector<weight_t>, std::vector<vertex_t>>
run_dsp_on_subgraph(const crs_graph& sub_crs,
                    const std::vector<vertex_t>& sub_to_orig,
                    vertex_t sub_source,
                    size_t n)
{
    // Run the DSP algorithm that does not use masks.
    auto sub_result = dijkstra_nomask<d_ary_heap_policy<>>(sub_crs, sub_source);
    const size_t sub_n = sub_to_orig.size();

    std::vector<weight_t> final_distance(n, std::numeric_limits<weight_t>::infinity());
    std::vector<vertex_t> final_pred(n, (std::numeric_limits<vertex_t>::max)());

    // Map the subgraph results back to the original graph.
    for (size_t i = 0; i < sub_n; ++i) {
        vertex_t orig = sub_to_orig[i];
        final_distance[orig] = sub_result.first[i];
        if (sub_result.second[i] != (std::numeric_limits<vertex_t>::max)())
            final_pred[orig] = sub_to_orig[sub_result.second[i]];
    }
    return { final_distance, final_pred };
}

//---------------------------------------------------------------------
// Function: dijkstra_filtered
//
// Description:
//   A convenience wrapper that takes the original Boost graph, mask vectors,
//   and original source vertex index. It then constructs a filtered subgraph,
//   obtains the mappings, runs the no-mask DSP on the subgraph, and maps the 
//   results back to the original graph indexing.
// 
// Note: This function uses thread_local buffers to avoid repeated allocations.
//---------------------------------------------------------------------
template <typename BoostGraph>
std::pair<std::vector<weight_t>, std::vector<vertex_t>>
dijkstra_filtered(const BoostGraph& bg,
                  const std::vector<uint8_t>& vertex_mask,
                  const std::vector<uint8_t>& edge_mask,
                  vertex_t source_orig)
{
    const size_t n = boost::num_vertices(bg);

    // Thread-local buffers to reuse between calls.
    thread_local std::vector<vertex_t> orig_to_sub;
    thread_local std::vector<vertex_t> sub_to_orig;

    crs_graph sub_crs;
    build_filtered_crs(bg, vertex_mask, edge_mask, sub_crs, orig_to_sub, sub_to_orig);

    // If the source vertex is not valid in the original graph, return all infinities.
    if (source_orig >= n || vertex_mask[source_orig] == 0) {
        std::vector<weight_t> distances(n, std::numeric_limits<weight_t>::infinity());
        std::vector<vertex_t> preds(n, (std::numeric_limits<vertex_t>::max)());
        return { distances, preds };
    }
    // Map the original source to subgraph coordinates.
    vertex_t sub_source = orig_to_sub[source_orig];

    // Run the Dijkstra algorithm on the subgraph and then map the results back.
    return run_dsp_on_subgraph(sub_crs, sub_to_orig, sub_source, n);
}

}} // namespace stk::graph
*/

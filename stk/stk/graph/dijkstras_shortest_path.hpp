//!
//! @file dsp.hpp
//! @brief SIMD-accelerated Dijkstra's algorithm for CRS graph.

#pragma once

#include <immintrin.h>
#include <vector>
#include <queue>
#include <limits>
#include <cstddef>
#include <utility>
#include <functional>
#include "stk/graph/crs_graph.hpp"

namespace stk::graph {
	//! Templated Dijkstra implementation with customizable priority queue and predecessor tracking
	template <typename QueuePolicy = default_priority_queue_policy>
	inline std::pair<std::vector<weight_t>, std::vector<vertex_t>> dijkstra(
		const crs_graph&                 graph,
		vertex_t                         source,
		const std::vector<std::uint8_t>& vertex_mask,
		const std::vector<std::uint8_t>& edge_mask )
	{
		const std::size_t     num_vertices = graph.row_starts.size() - 1;
		std::vector<weight_t> distance( num_vertices, std::numeric_limits<weight_t>::infinity() );
		std::vector<vertex_t> predecessor( num_vertices, ( std::numeric_limits<vertex_t>::max )() );

		weight_queue_t<QueuePolicy> open;

		if( !vertex_mask[source] )
			return { distance, predecessor };

		distance[source] = weight_t{};
		open.emplace( weight_t{}, source );

		while( !open.empty() )
		{
			auto [dist_u, u] = open.top();
			open.pop();
			if( !vertex_mask[u] )
				continue;

			std::size_t begin = graph.row_starts[u];
			std::size_t end = graph.row_starts[u + 1];
			std::size_t len = end - begin;

			std::size_t i = 0;
			for( ; i + 8 <= len; i += 8 )
			{
				__m256   dist_vec = _mm256_set1_ps( dist_u );
				__m256   wts = _mm256_loadu_ps( &graph.weights[begin + i] );
				weight_t curr_dists[8];
				uint8_t  cached_mask[8]; // Cache for the vertex mask for these 8 edges

				// Cache the mask values and load the current distances in one pass
				for( auto j = 0ULL; j < 8; ++j )
				{
					auto v = graph.targets[begin + i + j];
					cached_mask[j] = vertex_mask[v]; // Assume vertex_mask is a vector<uint8_t>
					curr_dists[j] = cached_mask[j] ? distance[v] : std::numeric_limits<weight_t>::infinity();
				}

				__m256 dists = _mm256_loadu_ps( curr_dists );
				__m256 new_dists = _mm256_add_ps( dist_vec, wts );
				__m256 mask = _mm256_cmp_ps( new_dists, dists, _CMP_LT_OS );

				// Use the cached mask values later to decide whether to update
				for( auto j = 0ULL; j < 8; ++j )
				{
					auto v = graph.targets[begin + i + j];
					// Use cached_mask[j] rather than another call to vertex_mask[v]
					if( edge_mask[begin + i + j] && cached_mask[j] )
					{
						if( ( (weight_t*)&mask )[j] )
						{
							distance[v] = ( (weight_t*)&new_dists )[j];
							predecessor[v] = u;
							open.emplace( distance[v], v );
						}
					}
				}
			}


			for( ; i < len; ++i )
			{
				std::size_t ei = begin + i;
				vertex_t    v = graph.targets[ei];
				weight_t    w = graph.weights[ei];
				if( !edge_mask[ei] || !vertex_mask[v] )
					continue;

				weight_t alt = dist_u + w;
				if( alt < distance[v] )
				{
					distance[v] = alt;
					predecessor[v] = u;
					open.emplace( alt, v );
				}
			}
		}

		return { distance, predecessor };
	}

	// Templated Dijkstra implementation with customizable priority queue and predecessor tracking.
	// Now with a pre-filtering step for edges to avoid per-element mask checks in the inner loop.
	template <typename QueuePolicy = default_priority_queue_policy>
	inline std::pair<std::vector<weight_t>, std::vector<vertex_t>> dijkstra_pre_filter(
		const crs_graph&            graph,
		vertex_t                    source,
		const std::vector<uint8_t>& vertex_mask, // using uint8_t for faster access
		const std::vector<uint8_t>& edge_mask )
	{
		const std::size_t     num_vertices = graph.row_starts.size() - 1;
		std::vector<weight_t> distance( num_vertices, std::numeric_limits<weight_t>::infinity() );
		std::vector<vertex_t> predecessor( num_vertices, ( std::numeric_limits<vertex_t>::max )() );

		weight_queue_t<QueuePolicy> open;

		if( !vertex_mask[source] )
			return { distance, predecessor };

		distance[source] = weight_t{};
		open.emplace( weight_t{}, source );

		while( !open.empty() )
		{
			auto [dist_u, u] = open.top();
			open.pop();
			if( !vertex_mask[u] )
				continue;

			const std::size_t row_start = graph.row_starts[u];
			const std::size_t row_end = graph.row_starts[u + 1];
			const std::size_t row_length = row_end - row_start;

			// === Pre-filter the edges from u ===
			// Build a temporary vector that holds indices in the original arrays for valid edges.
			std::vector<std::size_t> valid_edge_indices;
			valid_edge_indices.reserve( row_length );
			for( std::size_t i = row_start; i < row_end; ++i )
			{
				auto v = graph.targets[i];
				// Only consider the edge if the edge itself and the target vertex pass the mask.
				if( edge_mask[i] && vertex_mask[v] )
					valid_edge_indices.push_back( i );
			}

			const std::size_t filtered_size = valid_edge_indices.size();
			std::size_t       i = 0;
			// === Vectorized loop over the filtered edges ===
			for( ; i + 8 <= filtered_size; i += 8 )
			{
				// Process eight valid edges at a time.
				__m256 dist_vec = _mm256_set1_ps( dist_u );

				// Load weights for these edges from pre-filtered indices.
				float weight_array[8];
				for( std::size_t j = 0; j < 8; ++j )
				{
					weight_array[j] = graph.weights[valid_edge_indices[i + j]];
				}
				__m256 wts = _mm256_loadu_ps( weight_array );

				// Load current distances for the target vertices.
				weight_t curr_dists[8];
				for( std::size_t j = 0; j < 8; ++j )
				{
					auto idx = valid_edge_indices[i + j];
					auto v = graph.targets[idx];
					curr_dists[j] = distance[v];
				}
				__m256 dists = _mm256_loadu_ps( curr_dists );

				__m256 new_dists = _mm256_add_ps( dist_vec, wts );

				// Compare new distances with the current distances.
				__m256 cmp = _mm256_cmp_ps( new_dists, dists, _CMP_LT_OS );

				// Update entries where the new distance is smaller.
				for( std::size_t j = 0; j < 8; ++j )
				{
					// The cast and index access here extracts the j'th float from cmp.
					if( ( (weight_t*)&cmp )[j] )
					{
						auto idx = valid_edge_indices[i + j];
						auto v = graph.targets[idx];
						distance[v] = ( (weight_t*)&new_dists )[j];
						predecessor[v] = u;
						open.emplace( distance[v], v );
					}
				}
			}

			// === Process any remaining edges in scalar code ===
			for( ; i < filtered_size; ++i )
			{
				std::size_t idx = valid_edge_indices[i];
				auto        v = graph.targets[idx];
				weight_t    alt = dist_u + graph.weights[idx];
				if( alt < distance[v] )
				{
					distance[v] = alt;
					predecessor[v] = u;
					open.emplace( alt, v );
				}
			}
		}

		return { distance, predecessor };
	}

	// Templated Dijkstra implementation with AVX2 vectorization and per-thread prefiltered buffer.
	template <typename QueuePolicy = default_priority_queue_policy>
	inline std::pair<std::vector<weight_t>, std::vector<vertex_t>> dijkstra_prefilter_thread(
		const crs_graph&            graph,
		vertex_t                    source,
		const std::vector<uint8_t>& vertex_mask,
		const std::vector<uint8_t>& edge_mask )
	{
		const std::size_t     num_vertices = graph.row_starts.size() - 1;
		std::vector<weight_t> distance( num_vertices, std::numeric_limits<weight_t>::infinity() );
		std::vector<vertex_t> predecessor( num_vertices, ( std::numeric_limits<vertex_t>::max )() );

		weight_queue_t<QueuePolicy> open;
		if( !vertex_mask[source] )
			return { distance, predecessor };

		distance[source] = weight_t{};
		open.emplace( weight_t{}, source );

		// Thread-local buffer to avoid dynamic allocation on each call.
		// This buffer will hold the indices of valid edges for the current vertex.
		// Reserve an initial capacity (adjust as needed).
		thread_local std::vector<std::size_t> prefilter_buffer;
		prefilter_buffer.clear();

		while( !open.empty() )
		{
			auto [dist_u, u] = open.top();
			open.pop();
			if( !vertex_mask[u] )
				continue;

			const std::size_t row_start = graph.row_starts[u];
			const std::size_t row_end = graph.row_starts[u + 1];
			const std::size_t row_length = row_end - row_start;
			prefilter_buffer.clear();
			prefilter_buffer.reserve( row_length );

			// Build prefiltered indices for vertex u.
			for( std::size_t i = row_start; i < row_end; ++i )
			{
				auto v = graph.targets[i];
				if( edge_mask[i] && vertex_mask[v] )
					prefilter_buffer.push_back( i );
			}

			const std::size_t filtered_size = prefilter_buffer.size();
			std::size_t       i = 0;
			// AVX2 vectorized loop over the valid edges.
			for( ; i + 8 <= filtered_size; i += 8 )
			{
				__m256 dist_vec = _mm256_set1_ps( dist_u );

				float weight_array[8];
				for( std::size_t j = 0; j < 8; ++j )
					weight_array[j] = graph.weights[prefilter_buffer[i + j]];
				__m256 wts = _mm256_loadu_ps( weight_array );

				weight_t curr_dists[8];
				for( std::size_t j = 0; j < 8; ++j )
				{
					auto idx = prefilter_buffer[i + j];
					auto v = graph.targets[idx];
					curr_dists[j] = distance[v];
				}
				__m256 dists = _mm256_loadu_ps( curr_dists );
				__m256 new_dists = _mm256_add_ps( dist_vec, wts );
				__m256 cmp = _mm256_cmp_ps( new_dists, dists, _CMP_LT_OS );

				for( std::size_t j = 0; j < 8; ++j )
				{
					if( ( (weight_t*)&cmp )[j] )
					{
						auto idx = prefilter_buffer[i + j];
						auto v = graph.targets[idx];
						distance[v] = ( (weight_t*)&new_dists )[j];
						predecessor[v] = u;
						open.emplace( distance[v], v );
					}
				}
			}

			// Process remainder edges in scalar.
			for( ; i < filtered_size; ++i )
			{
				std::size_t idx = prefilter_buffer[i];
				auto        v = graph.targets[idx];
				weight_t    alt = dist_u + graph.weights[idx];
				if( alt < distance[v] )
				{
					distance[v] = alt;
					predecessor[v] = u;
					open.emplace( alt, v );
				}
			}
		}
		return { distance, predecessor };
	}

	// A SIMD-accelerated Dijkstra's algorithm for CRS graphs with no masks.
	// Assumes that every vertex and edge is valid.
	template <typename QueuePolicy = default_priority_queue_policy>
	inline std::pair<std::vector<weight_t>, std::vector<vertex_t>> dijkstra_nomask(
		const crs_graph& graph,
		vertex_t         source )
	{
		const std::size_t     num_vertices = graph.row_starts.size() - 1;
		std::vector<weight_t> distance( num_vertices, std::numeric_limits<weight_t>::infinity() );
		std::vector<vertex_t> predecessor( num_vertices, ( std::numeric_limits<vertex_t>::max )() );

		// Note: weight_queue_t is assumed to be defined in your library.
		weight_queue_t<QueuePolicy> open;

		distance[source] = weight_t{};
		open.emplace( weight_t{}, source );

		while( !open.empty() )
		{
			auto [dist_u, u] = open.top();
			open.pop();

			std::size_t begin = graph.row_starts[u];
			std::size_t end = graph.row_starts[u + 1];
			std::size_t len = end - begin;

			std::size_t i = 0;
			// Process edges in chunks of 8 using AVX2 intrinsics.
			for( ; i + 8 <= len; i += 8 )
			{
				__m256 dist_vec = _mm256_set1_ps( dist_u );
				__m256 wts = _mm256_loadu_ps( &graph.weights[begin + i] );

				weight_t curr_dists[8];
				for( std::size_t j = 0; j < 8; ++j )
				{
					auto v = graph.targets[begin + i + j];
					curr_dists[j] = distance[v];
				}
				__m256 dists = _mm256_loadu_ps( curr_dists );
				__m256 new_dists = _mm256_add_ps( dist_vec, wts );
				__m256 cmp = _mm256_cmp_ps( new_dists, dists, _CMP_LT_OS );

				for( std::size_t j = 0; j < 8; ++j )
				{
					auto v = graph.targets[begin + i + j];
					if( ( (weight_t*)&cmp )[j] )
					{
						distance[v] = ( (weight_t*)&new_dists )[j];
						predecessor[v] = u;
						open.emplace( distance[v], v );
					}
				}
			}

			// Process any remaining edges in scalar code.
			for( ; i < len; ++i )
			{
				std::size_t ei = begin + i;
				auto        v = graph.targets[ei];
				weight_t    alt = dist_u + graph.weights[ei];
				if( alt < distance[v] )
				{
					distance[v] = alt;
					predecessor[v] = u;
					open.emplace( alt, v );
				}
			}
		}

		return { distance, predecessor };
	}

} // namespace stk::graph

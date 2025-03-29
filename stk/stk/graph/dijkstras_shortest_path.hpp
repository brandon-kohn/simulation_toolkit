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
		const crs_graph&         graph,
		vertex_t                 source,
		const std::vector<bool>& vertex_mask,
		const std::vector<bool>& edge_mask )
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
				__m256 dist_vec = _mm256_set1_ps( dist_u );
				__m256 wts = _mm256_loadu_ps( &graph.weights[begin + i] );

				weight_t curr_dists[8];
				for( auto j = 0ULL; j < 8; ++j )
				{
					auto v = graph.targets[begin + i + j];
					curr_dists[j] = vertex_mask[v] ? distance[v] : std::numeric_limits<weight_t>::infinity();
				}
				__m256 dists = _mm256_loadu_ps( curr_dists );
				__m256 new_dists = _mm256_add_ps( dist_vec, wts );
				__m256 mask = _mm256_cmp_ps( new_dists, dists, _CMP_LT_OS );

				for( auto j = 0ULL; j < 8; ++j )
				{
					auto v = graph.targets[begin + i + j];
					if( edge_mask[begin + i + j] && vertex_mask[v] )
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

} // namespace stk::graph

//!
//! @file astar.hpp
//! @brief SIMD-accelerated A* search algorithm for CRS graphs.

#pragma once

#include <immintrin.h>
#include <vector>
#include <queue>
#include <limits>
#include <functional>
#include <cassert>
#include <cstddef>

#include "stk/graph/crs_graph.hpp"

namespace stk::graph {

	//! A* search with customizable priority queue and predecessor tracking
	template <typename Heuristic, typename QueuePolicy = default_priority_queue_policy>
	inline std::pair<std::vector<weight_t>, std::vector<vertex_t>> astar(
		const crs_graph&         graph,
		vertex_t                 source,
		vertex_t                 goal,
		const std::vector<std::uint8_t>& vertex_mask,
		const std::vector<std::uint8_t>& edge_mask,
		Heuristic&&              heuristic )
	{
		const std::size_t     num_vertices = graph.row_starts.size() - 1;
		std::vector<weight_t> distance( num_vertices, std::numeric_limits<weight_t>::infinity() );
		std::vector<vertex_t> predecessor( num_vertices, static_cast<vertex_t>( -1 ) );

		weight_queue_t<QueuePolicy> open;

		if( !vertex_mask[source] )
			return { distance, predecessor };

		distance[source] = weight_t{};
		open.emplace( heuristic( source ), source );

		while( !open.empty() )
		{
			auto [f_u, u] = open.top();
			open.pop();
			if( u == goal )
				break;
			if( !vertex_mask[u] )
				continue;

			std::size_t begin = graph.row_starts[u];
			std::size_t end = graph.row_starts[u + 1];
			std::size_t len = end - begin;

			std::size_t i = 0;
			for( ; i + 8 <= len; i += 8 )
			{
				__m256 g_u = _mm256_set1_ps( distance[u] );
				__m256 wts = _mm256_loadu_ps( &graph.weights[begin + i] );

				weight_t curr_dists[8];
				weight_t heuristics[8];
				for( int j = 0; j < 8; ++j )
				{
					vertex_t v = graph.targets[begin + i + j];
					auto     vMask = vertex_mask[v];
					curr_dists[j] = vMask ? distance[v] : std::numeric_limits<weight_t>::infinity();
					heuristics[j] = vMask ? heuristic( v ) : 0.0f;
				}

				__m256 g_cand = _mm256_add_ps( g_u, wts );
				__m256 g_curr = _mm256_loadu_ps( curr_dists );
				__m256 mask = _mm256_cmp_ps( g_cand, g_curr, _CMP_LT_OS );

				for( auto j = 0ULL; j < 8; ++j )
				{
					vertex_t v = graph.targets[begin + i + j];
					if( edge_mask[begin + i + j] && vertex_mask[v] )
					{
						if( ( (weight_t*)&mask )[j] )
						{
							weight_t g = ( (weight_t*)&g_cand )[j];
							distance[v] = g;
							predecessor[v] = u;
							weight_t f = g + heuristics[j];
							open.emplace( f, v );
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

				weight_t g = distance[u] + w;
				if( g < distance[v] )
				{
					distance[v] = g;
					predecessor[v] = u;
					weight_t f = g + heuristic( v );
					open.emplace( f, v );
				}
			}
		}

		return { distance, predecessor };
	}

} // namespace stk::graph

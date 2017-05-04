//
//=======================================================================
// Copyright (c) 2004 Kristopher Beevers
//! Copyright © 2017 Brandon Kohn with ideas from StackOverflow.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//=======================================================================
//
#ifndef STK_GRAPH_STOPPABLE_ASTAR_SEARCH_HPP
#define STK_GRAPH_STOPPABLE_ASTAR_SEARCH_HPP

#include <boost/graph/astar_search.hpp>
#include <stk/graph/stoppable_breadth_first_search.hpp>

namespace boost {

	template <class Visitor, class Graph>
	struct StoppableAStarVisitorConcept : AStarVisitorConcept<Visitor, Graph> {
		void constraints()
		{
			AStarVisitorConcept<Visitor, Graph>::constraints();
			bool b = vis.should_stop(u, g);
		}
	};
	
	template <typename Visitors = null_visitor>
	struct stoppable_astar_visitor : public stoppable_bfs_visitor<Visitors>
	{
		stoppable_astar_visitor() {}
		stoppable_astar_visitor(Visitors vis) : stoppable_bfs_visitor<Visitors>(vis) {}
		
		template <class Edge, class Graph>
		void edge_relaxed(Edge e, const Graph& g) 
		{
			invoke_visitors(this->m_vis, e, g, on_edge_relaxed());
		}
		template <class Edge, class Graph>
		void edge_not_relaxed(Edge e, const Graph& g) 
		{
			invoke_visitors(this->m_vis, e, g, on_edge_not_relaxed());
		}

	private:
		template <class Edge, class Graph>
		void tree_edge(Edge e, const Graph& g) {}
		template <class Edge, class Graph>
		void non_tree_edge(Edge e, const Graph& g) {}
	};

	template <class Visitors>
	inline stoppable_astar_visitor<Visitors> make_stoppable_astar_visitor(Visitors vis) 
	{
		return stoppable_astar_visitor<Visitors>(vis);
	}
	typedef stoppable_astar_visitor<> default_stoppable_astar_visitor;
	
	namespace detail {

		template <class AStarHeuristic, class UniformCostVisitor,
			class UpdatableQueue, class PredecessorMap,
			class CostMap, class DistanceMap, class WeightMap,
			class ColorMap, class BinaryFunction,
			class BinaryPredicate>
		struct stoppable_astar_bfs_visitor
		{
			typedef typename property_traits<CostMap>::value_type C;
			typedef typename property_traits<ColorMap>::value_type ColorValue;
			typedef color_traits<ColorValue> Color;
			typedef typename property_traits<DistanceMap>::value_type distance_type;

			stoppable_astar_bfs_visitor(AStarHeuristic h, UniformCostVisitor vis,
				UpdatableQueue& Q, PredecessorMap p,
				CostMap c, DistanceMap d, WeightMap w,
				ColorMap col, BinaryFunction combine,
				BinaryPredicate compare, C zero)
			: m_h(h), m_vis(vis), m_Q(Q), m_predecessor(p), m_cost(c),
			  m_distance(d), m_weight(w), m_color(col),
			  m_combine(combine), m_compare(compare), m_zero(zero)
			{}
			
			template <class Vertex, class Graph>
			void initialize_vertex(Vertex u, const Graph& g) 
			{
				m_vis.initialize_vertex(u, g);
			}
			template <class Vertex, class Graph>
			void discover_vertex(Vertex u, const Graph& g) 
			{
				m_vis.discover_vertex(u, g);
			}
			template <class Vertex, class Graph>
			void examine_vertex(Vertex u, const Graph& g) 
			{
				m_vis.examine_vertex(u, g);
			}
			template <class Vertex, class Graph>
			void finish_vertex(Vertex u, const Graph& g) 
			{
				m_vis.finish_vertex(u, g);
			}
			template <class Edge, class Graph>
			void examine_edge(Edge e, const Graph& g) 
			{
				if (m_compare(get(m_weight, e), m_zero))
					BOOST_THROW_EXCEPTION(negative_edge());
				m_vis.examine_edge(e, g);
			}
			template <class Edge, class Graph>
			void non_tree_edge(Edge, const Graph&) {}
			
			template <class Edge, class Graph>
			void tree_edge(Edge e, const Graph& g) 
			{
				using boost::get;
				bool m_decreased = relax(e, g, m_weight, m_predecessor, m_distance, m_combine, m_compare);

				if (m_decreased) 
				{
					m_vis.edge_relaxed(e, g);
					put(m_cost, target(e, g), m_combine(get(m_distance, target(e, g)), m_h(target(e, g))));
				}
				else
					m_vis.edge_not_relaxed(e, g);
			}
			
			template <class Edge, class Graph>
			void gray_target(Edge e, const Graph& g) 
			{
				using boost::get;
				bool m_decreased = relax(e, g, m_weight, m_predecessor, m_distance, m_combine, m_compare);

				if (m_decreased) 
				{
					put(m_cost, target(e, g), m_combine(get(m_distance, target(e, g)), m_h(target(e, g))));
					m_Q.update(target(e, g));
					m_vis.edge_relaxed(e, g);
				}
				else
					m_vis.edge_not_relaxed(e, g);
			}
			
			template <class Edge, class Graph>
			void black_target(Edge e, const Graph& g) 
			{
				using boost::get;
				bool m_decreased = relax(e, g, m_weight, m_predecessor, m_distance, m_combine, m_compare);

				if (m_decreased) 
				{
					m_vis.edge_relaxed(e, g);
					put(m_cost, target(e, g), m_combine(get(m_distance, target(e, g)), m_h(target(e, g))));
					m_Q.push(target(e, g));
					put(m_color, target(e, g), Color::gray());
					m_vis.black_target(e, g);
				}
				else
					m_vis.edge_not_relaxed(e, g);
			}

			template <typename Vertex, typename Graph>
			bool should_stop(Vertex v, Graph& g)
			{ 
				return m_vis.should_stop(v, g);
			}
			
			AStarHeuristic m_h;
			UniformCostVisitor m_vis;
			UpdatableQueue& m_Q;
			PredecessorMap m_predecessor;
			CostMap m_cost;
			DistanceMap m_distance;
			WeightMap m_weight;
			ColorMap m_color;
			BinaryFunction m_combine;
			BinaryPredicate m_compare;
			C m_zero;
		};

	} // namespace detail
	
	template <typename VertexListGraph, typename AStarHeuristic, typename StoppableAStarVisitor, typename PredecessorMap, typename CostMap, typename DistanceMap, typename WeightMap, typename ColorMap, typename VertexIndexMap, typename CompareFunction, typename CombineFunction, typename CostZero>
	inline void stoppable_astar_search_no_init(const VertexListGraph &g, typename graph_traits<VertexListGraph>::vertex_descriptor s, AStarHeuristic h, StoppableAStarVisitor vis, PredecessorMap predecessor, CostMap cost, DistanceMap distance, WeightMap weight, ColorMap color, VertexIndexMap index_map, CompareFunction compare, CombineFunction combine, CostZero zero)
	{
		typedef typename graph_traits<VertexListGraph>::vertex_descriptor Vertex;
		typedef boost::vector_property_map<std::size_t, VertexIndexMap> IndexInHeapMap;
		IndexInHeapMap index_in_heap(index_map);
		typedef d_ary_heap_indirect<Vertex, 4, IndexInHeapMap, CostMap, CompareFunction> MutableQueue;
		MutableQueue Q(cost, index_in_heap, compare);

		detail::stoppable_astar_bfs_visitor<AStarHeuristic, StoppableAStarVisitor, MutableQueue, PredecessorMap, CostMap, DistanceMap, WeightMap, ColorMap, CombineFunction, CompareFunction> bfs_vis(h, vis, Q, predecessor, cost, distance, weight, color, combine, compare, zero);
		stoppable_breadth_first_visit(g, s, Q, bfs_vis, color);
	}
	
	template <typename VertexListGraph, typename AStarHeuristic, typename StoppableAStarVisitor, typename PredecessorMap, typename CostMap, typename DistanceMap, typename WeightMap, typename CompareFunction, typename CombineFunction, typename CostZero>
	inline void stoppable_astar_search_no_init_tree(const VertexListGraph &g, typename graph_traits<VertexListGraph>::vertex_descriptor s, AStarHeuristic h, StoppableAStarVisitor vis, PredecessorMap predecessor, CostMap cost, DistanceMap distance, WeightMap weight, CompareFunction compare, CombineFunction combine, CostZero zero)
	{
		typedef typename graph_traits<VertexListGraph>::vertex_descriptor Vertex;
		typedef typename property_traits<DistanceMap>::value_type Distance;
		typedef d_ary_heap_indirect<
			std::pair<Distance, Vertex>,
			4,
			null_property_map<std::pair<Distance, Vertex>, std::size_t>,
			function_property_map<graph_detail::select1st<Distance, Vertex>, std::pair<Distance, Vertex> >,
			CompareFunction>
			MutableQueue;
		MutableQueue Q(make_function_property_map<std::pair<Distance, Vertex> >(graph_detail::select1st<Distance, Vertex>()), null_property_map<std::pair<Distance, Vertex>, std::size_t>(), compare);

		vis.discover_vertex(s, g);
		Q.push(std::make_pair(get(cost, s), s));
		while (!Q.empty())
		{
			Vertex v;
			Distance v_rank;
			boost::tie(v_rank, v) = Q.top();
			Q.pop();
			if (vis.should_stop(v, g))
				return;
			vis.examine_vertex(v, g);
			BGL_FORALL_OUTEDGES_T(v, e, g, VertexListGraph) 
			{
				Vertex w = target(e, g);
				vis.examine_edge(e, g);
				Distance e_weight = get(weight, e);
				if (compare(e_weight, zero))
					BOOST_THROW_EXCEPTION(negative_edge());
				bool decreased = relax(e, g, weight, predecessor, distance,	combine, compare);
				Distance w_d = combine(get(distance, v), e_weight);
				if (decreased)
				{
					vis.edge_relaxed(e, g);
					Distance w_rank = combine(get(distance, w), h(w));
					put(cost, w, w_rank);
					vis.discover_vertex(w, g);
					Q.push(std::make_pair(w_rank, w));
				}
				else
					vis.edge_not_relaxed(e, g);
			}
			vis.finish_vertex(v, g);
		}
	}

	// Non-named parameter interface
	template <typename VertexListGraph, typename AStarHeuristic, typename StoppableAStarVisitor, typename PredecessorMap, typename CostMap, typename DistanceMap, typename WeightMap, typename VertexIndexMap, typename ColorMap, typename CompareFunction, typename CombineFunction, typename CostInf, typename CostZero>
	inline void stoppable_astar_search(const VertexListGraph &g, typename graph_traits<VertexListGraph>::vertex_descriptor s, AStarHeuristic h, StoppableAStarVisitor vis, PredecessorMap predecessor, CostMap cost, DistanceMap distance, WeightMap weight, VertexIndexMap index_map, ColorMap color, CompareFunction compare, CombineFunction combine, CostInf inf, CostZero zero)
	{
		typedef typename property_traits<ColorMap>::value_type ColorValue;
		typedef color_traits<ColorValue> Color;
		typename graph_traits<VertexListGraph>::vertex_iterator ui, ui_end;
		for (boost::tie(ui, ui_end) = vertices(g); ui != ui_end; ++ui) {
			put(color, *ui, Color::white());
			put(distance, *ui, inf);
			put(cost, *ui, inf);
			put(predecessor, *ui, *ui);
			vis.initialize_vertex(*ui, g);
		}
		put(distance, s, zero);
		put(cost, s, h(s));

		stoppable_astar_search_no_init(g, s, h, vis, predecessor, cost, distance, weight, color, index_map, compare, combine, zero);
	}

	// Non-named parameter interface
	template <typename VertexListGraph, typename AStarHeuristic, typename StoppableAStarVisitor, typename PredecessorMap, typename CostMap, typename DistanceMap, typename WeightMap, typename CompareFunction, typename CombineFunction, typename CostInf, typename CostZero>
	inline void stoppable_astar_search_tree(const VertexListGraph &g, typename graph_traits<VertexListGraph>::vertex_descriptor s, AStarHeuristic h, StoppableAStarVisitor vis, PredecessorMap predecessor, CostMap cost, DistanceMap distance, WeightMap weight, CompareFunction compare, CombineFunction combine, CostInf inf, CostZero zero)
	{
		typename graph_traits<VertexListGraph>::vertex_iterator ui, ui_end;
		for (boost::tie(ui, ui_end) = vertices(g); ui != ui_end; ++ui) 
		{
			put(distance, *ui, inf);
			put(cost, *ui, inf);
			put(predecessor, *ui, *ui);
			vis.initialize_vertex(*ui, g);
		}
		put(distance, s, zero);
		put(cost, s, h(s));

		stoppable_astar_search_no_init_tree(g, s, h, vis, predecessor, cost, distance, weight, compare, combine, zero);
	}

	// Named parameter interfaces
	template <typename VertexListGraph, typename AStarHeuristic, typename P, typename T, typename R>
	inline void stoppable_astar_search(const VertexListGraph &g, typename graph_traits<VertexListGraph>::vertex_descriptor s, AStarHeuristic h, const bgl_named_params<P, T, R>& params)
	{
		using namespace boost::graph::keywords;
		typedef bgl_named_params<P, T, R> params_type;
		BOOST_GRAPH_DECLARE_CONVERTED_PARAMETERS(params_type, params)

		// Distance type is the value type of the distance map if there is one,
		// otherwise the value type of the weight map.
		typedef	typename detail::override_const_property_result<arg_pack_type, tag::weight_map, edge_weight_t, VertexListGraph>::type weight_map_type;
		typedef typename boost::property_traits<weight_map_type>::value_type W;
		typedef	typename detail::map_maker<VertexListGraph, arg_pack_type, tag::distance_map, W>::map_type distance_map_type;
		typedef typename boost::property_traits<distance_map_type>::value_type D;
		const D inf = arg_pack[_distance_inf || detail::get_max<D>()];

		stoppable_astar_search
		(g, s, h,
			arg_pack[_visitor | make_stoppable_astar_visitor(null_visitor())],
			arg_pack[_predecessor_map | dummy_property_map()],
			detail::make_property_map_from_arg_pack_gen<tag::rank_map, D>(D())(g, arg_pack),
			detail::make_property_map_from_arg_pack_gen<tag::distance_map, W>(W())(g, arg_pack),
			detail::override_const_property(arg_pack, _weight_map, g, edge_weight),
			detail::override_const_property(arg_pack, _vertex_index_map, g, vertex_index),
			detail::make_color_map_from_arg_pack(g, arg_pack),
			arg_pack[_distance_compare | std::less<D>()],
			arg_pack[_distance_combine | closed_plus<D>(inf)],
			inf,
			arg_pack[_distance_zero | D()]);
	}

	// Named parameter interfaces
	template <typename VertexListGraph, typename AStarHeuristic, typename P, typename T, typename R>
	inline void stoppable_astar_search_tree(const VertexListGraph &g, typename graph_traits<VertexListGraph>::vertex_descriptor s, AStarHeuristic h, const bgl_named_params<P, T, R>& params)
	{
		using namespace boost::graph::keywords;
		typedef bgl_named_params<P, T, R> params_type;
		BOOST_GRAPH_DECLARE_CONVERTED_PARAMETERS(params_type, params)

		// Distance type is the value type of the distance map if there is one,
		// otherwise the value type of the weight map.
		typedef typename detail::override_const_property_result<arg_pack_type, tag::weight_map, edge_weight_t, VertexListGraph>::type weight_map_type;
		typedef typename boost::property_traits<weight_map_type>::value_type W;
		typedef typename detail::map_maker<VertexListGraph, arg_pack_type, tag::distance_map, W>::map_type distance_map_type;
		typedef typename boost::property_traits<distance_map_type>::value_type D;
		const D inf = arg_pack[_distance_inf || detail::get_max<D>()];

		stoppable_astar_search_tree
		(g, s, h,
			arg_pack[_visitor | make_stoppable_astar_visitor(null_visitor())],
			arg_pack[_predecessor_map | dummy_property_map()],
			detail::make_property_map_from_arg_pack_gen<tag::rank_map, D>(D())(g, arg_pack),
			detail::make_property_map_from_arg_pack_gen<tag::distance_map, W>(W())(g, arg_pack),
			detail::override_const_property(arg_pack, _weight_map, g, edge_weight),
			arg_pack[_distance_compare | std::less<D>()],
			arg_pack[_distance_combine | closed_plus<D>(inf)],
			inf,
			arg_pack[_distance_zero | D()]);
	}

	template <typename VertexListGraph, typename AStarHeuristic, typename P, typename T, typename R>
	inline void stoppable_astar_search_no_init(const VertexListGraph &g, typename graph_traits<VertexListGraph>::vertex_descriptor s, AStarHeuristic h, const bgl_named_params<P, T, R>& params)
	{
		using namespace boost::graph::keywords;
		typedef bgl_named_params<P, T, R> params_type;
		BOOST_GRAPH_DECLARE_CONVERTED_PARAMETERS(params_type, params)
		typedef typename detail::override_const_property_result<arg_pack_type, tag::weight_map, edge_weight_t, VertexListGraph>::type weight_map_type;
		typedef typename boost::property_traits<weight_map_type>::value_type D;
		const D inf = arg_pack[_distance_inf || detail::get_max<D>()];
		
		stoppable_astar_search_no_init
		(g, s, h,
			arg_pack[_visitor | make_stoppable_astar_visitor(null_visitor())],
			arg_pack[_predecessor_map | dummy_property_map()],
			detail::make_property_map_from_arg_pack_gen<tag::rank_map, D>(D())(g, arg_pack),
			detail::make_property_map_from_arg_pack_gen<tag::distance_map, D>(D())(g, arg_pack),
			detail::override_const_property(arg_pack, _weight_map, g, edge_weight),
			detail::make_color_map_from_arg_pack(g, arg_pack),
			detail::override_const_property(arg_pack, _vertex_index_map, g, vertex_index),
			arg_pack[_distance_compare | std::less<D>()],
			arg_pack[_distance_combine | closed_plus<D>(inf)],			
			arg_pack[_distance_zero | D()]);
	}

	template <typename VertexListGraph, typename AStarHeuristic, typename P, typename T, typename R>
	inline void stoppable_astar_search_no_init_tree (const VertexListGraph &g, typename graph_traits<VertexListGraph>::vertex_descriptor s, AStarHeuristic h, const bgl_named_params<P, T, R>& params)
	{
		using namespace boost::graph::keywords;
		typedef bgl_named_params<P, T, R> params_type;
		BOOST_GRAPH_DECLARE_CONVERTED_PARAMETERS(params_type, params)
		typedef typename detail::override_const_property_result<arg_pack_type, tag::weight_map, edge_weight_t, VertexListGraph>::type weight_map_type;
		typedef typename boost::property_traits<weight_map_type>::value_type D;
		const D inf = arg_pack[_distance_inf || detail::get_max<D>()];
		stoppable_astar_search_no_init_tree
		(g, s, h,
			arg_pack[_visitor | make_stoppable_astar_visitor(null_visitor())],
			arg_pack[_predecessor_map | dummy_property_map()],
			detail::make_property_map_from_arg_pack_gen<tag::rank_map, D>(D())(g, arg_pack),
			detail::make_property_map_from_arg_pack_gen<tag::distance_map, D>(D())(g, arg_pack),
			detail::override_const_property(arg_pack, _weight_map, g, edge_weight),
			arg_pack[_distance_compare | std::less<D>()],
			arg_pack[_distance_combine | closed_plus<D>(inf)],
			inf,
			arg_pack[_distance_zero | D()]);
	}

} // namespace boost

#endif//! STK_GRAPH_STOPPABLE_ASTAR_SEARCH_HPP

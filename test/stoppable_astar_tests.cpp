//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#include <stk/geometry/geometry_kernel.hpp>
#include <stk/graph/stoppable_astar_search.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <geometrix/algorithm/euclidean_distance.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace stk;

namespace {
    enum class VertexType
    {
            Obstacle
        ,   Target
    };

    enum class EdgeType
    {
            Real
        ,   Virtual
    };

    struct VertexProperties
    {
        VertexProperties(const point2& p = point2(0 * units::si::meters, 0 * units::si::meters), bool isConcave = false, VertexType t = VertexType::Obstacle)
            : position(p)
            , isConcave(isConcave)
            , type(t)
        {}

        point2 position;
        bool isConcave{ false };
        VertexType type;
    };

    struct EdgeProperties
    {
        EdgeProperties(units::length weight = 0.0 * units::si::meters, EdgeType t = EdgeType::Virtual)
            : weight(weight)
            , type(t)
        {}

        units::length weight;
        EdgeType type;
    };

    using Graph = boost::adjacency_list
        <
        boost::vecS,
        boost::vecS,
        boost::directedS,
        VertexProperties,
        EdgeProperties
        >;

    using Vertex = Graph::vertex_descriptor;
    using Edge = Graph::edge_descriptor;
}

template <class Graph>
class distance_heuristic : public boost::astar_heuristic<Graph, ::units::length>
{
public:
    distance_heuristic(Graph& g, Vertex goal)
        : m_graph(g), m_goal(goal) {}

    ::units::length operator()(Vertex u) const
    {
        return point_point_distance(m_graph[m_goal].position, m_graph[u].position);
    }

private:
    Graph& m_graph;
    Vertex m_goal;
};

// visitor that terminates when we find the goal
template <class Vertex>
class astar_goal_visitor : public boost::default_stoppable_astar_visitor
{
public:
    astar_goal_visitor(Vertex goal, bool& visitorTerminated)
        : m_goal(goal)
        , m_visitorTerminated(visitorTerminated)
    {}

    template <class Graph>
    bool should_stop(Vertex u, Graph& /*g*/) const
    {
        GEOMETRIX_ASSERT(!m_visitorTerminated);//! This should only be called when this is false if termination works.
        if (u == m_goal)
        {
            m_visitorTerminated = true;
            return true;
        }

        return false;
    }

private:
    Vertex m_goal;
    bool&  m_visitorTerminated;
};

TEST(StoppableAstarTestSuite, stoppable_astar_search_ThreeNodeGraphSearchingFromV1ToV2_VisitorProperlyTerminatesSearch)
{
    using namespace boost;

    Graph g;
    auto p1 = point2{ 0.* boost::units::si::meters, 0.* boost::units::si::meters };
    auto p2 = point2{ 0.* boost::units::si::meters, 1.* boost::units::si::meters };
    auto p3 = point2{ 1.* boost::units::si::meters, 1.* boost::units::si::meters };
    auto v1 = boost::add_vertex(VertexProperties(p1, true, VertexType::Target), g);
    auto v2 = boost::add_vertex(VertexProperties(p2, true, VertexType::Target), g);
    auto v3 = boost::add_vertex(VertexProperties(p3, true, VertexType::Target), g);
    {
        auto weight = geometrix::point_point_distance(p1, p2);
        EdgeProperties props = { weight, EdgeType::Virtual };
        Edge e; bool b;
        boost::tie(e, b) = boost::add_edge(v1, v2, props, g);
    }
    {
        auto weight = geometrix::point_point_distance(p2, p3);
        EdgeProperties props = { weight, EdgeType::Virtual };
        Edge e; bool b;
        boost::tie(e, b) = boost::add_edge(v2, v3, props, g);
    }
    auto start = v1;
    auto goal = v2;

    std::vector<Vertex> preds(num_vertices(g));
    std::vector<::units::length> costs(preds.size());

    bool visitorTerminated = false;

    stoppable_astar_search(g, start,
        distance_heuristic<Graph>(g,goal),
        predecessor_map(&preds[0])
        .distance_map(&costs[0])
        .weight_map(boost::get(&EdgeProperties::weight, g))
        .visitor(astar_goal_visitor<Vertex>(goal, visitorTerminated))
        .distance_inf(std::numeric_limits<::units::length>::infinity())
        .distance_zero(0.0 * boost::units::si::meters)
    );

    EXPECT_TRUE(visitorTerminated);
}

TEST(StoppableAstarTestSuite, stoppable_astar_search_tree_ThreeNodeGraphSearchingFromV1ToV2_VisitorProperlyTerminatesSearch)
{
    using namespace boost;

    Graph g;
    auto p1 = point2{ 0.* boost::units::si::meters, 0.* boost::units::si::meters };
    auto p2 = point2{ 0.* boost::units::si::meters, 1.* boost::units::si::meters };
    auto p3 = point2{ 1.* boost::units::si::meters, 1.* boost::units::si::meters };
    auto v1 = boost::add_vertex(VertexProperties(p1, true, VertexType::Target), g);
    auto v2 = boost::add_vertex(VertexProperties(p2, true, VertexType::Target), g);
    auto v3 = boost::add_vertex(VertexProperties(p3, true, VertexType::Target), g);
    {
        auto weight = geometrix::point_point_distance(p1, p2);
        EdgeProperties props = { weight, EdgeType::Virtual };
        Edge e; bool b;
        boost::tie(e, b) = boost::add_edge(v1, v2, props, g);
    }
    {
        auto weight = geometrix::point_point_distance(p2, p3);
        EdgeProperties props = { weight, EdgeType::Virtual };
        Edge e; bool b;
        boost::tie(e, b) = boost::add_edge(v2, v3, props, g);
    }
    auto start = v1;
    auto goal = v2;

    std::vector<Vertex> preds(num_vertices(g));
    std::vector<::units::length> costs(preds.size());

    bool visitorTerminated = false;

    stoppable_astar_search_tree(g, start,
        distance_heuristic<Graph>(g, goal),
        predecessor_map(&preds[0])
        .distance_map(&costs[0])
        .weight_map(boost::get(&EdgeProperties::weight, g))
        .visitor(astar_goal_visitor<Vertex>(goal, visitorTerminated))
        .distance_inf(std::numeric_limits<::units::length>::infinity())
        .distance_zero(0.0 * boost::units::si::meters)
    );

    EXPECT_TRUE(visitorTerminated);
}

#include "stk/graph/crs_graph.hpp"
#include "stk/graph/dijkstras_shortest_path.hpp"
#include "stk/graph/astar.hpp"

using namespace stk::graph;

namespace {

	class CrsGraphTest : public ::testing::Test
	{
	protected:
		void SetUp() override
		{
			constexpr std::size_t N = 5;
			builder = std::make_unique<crs_graph_builder>( N, /*undirected=*/true );

			builder->set_position( 0, 0.0f, 0.0f );
			builder->set_position( 1, 1.0f, 0.0f );
			builder->set_position( 2, 1.0f, 1.0f );
			builder->set_position( 3, 0.0f, 1.0f );
			builder->set_position( 4, 0.5f, 0.5f );

			builder->add_edge( 0, 1, 1.0f );
			builder->add_edge( 1, 2, 1.0f );
			builder->add_edge( 2, 3, 1.0f );
			builder->add_edge( 3, 0, 1.0f );
			builder->add_edge( 0, 4, 0.7f );
			builder->add_edge( 4, 2, 0.7f );

			graph = builder->build();

			vertex_mask.assign( 5, true );
			edge_mask.assign( graph.targets.size(), true );
		}

		std::unique_ptr<crs_graph_builder> builder;
		crs_graph                          graph;
		std::vector<bool>                  vertex_mask;
		std::vector<bool>                  edge_mask;
	};

	TEST_F( CrsGraphTest, DijkstraDistances )
	{
		auto [dist, preds] = dijkstra<stk::graph::d_ary_heap_policy<>>( graph, 0, vertex_mask, edge_mask );
		EXPECT_NEAR( dist[0], 0.0f, 1e-5f );
		EXPECT_NEAR( dist[1], 1.0f, 1e-5f );
		EXPECT_NEAR( dist[2], 1.4f, 1e-5f ); // 0 -> 4 -> 2
		EXPECT_NEAR( dist[3], 1.0f, 1e-5f );
		EXPECT_NEAR( dist[4], 0.7f, 1e-5f );
	}

	TEST_F( CrsGraphTest, AStarMatchesDijkstra )
	{
		int  goal = 2;
		auto heuristic = [&]( int v ) -> float
		{
			float dx = graph.positions[v].first - graph.positions[goal].first;
			float dy = graph.positions[v].second - graph.positions[goal].second;
			return dx * dx + dy * dy;
		};

		auto [dist_astar, preds_astar] = astar( graph, 0, goal, vertex_mask, edge_mask, heuristic );
		auto [dist_dsp, preds_dsp] = dijkstra<stk::graph::d_ary_heap_policy<>>( graph, 0, vertex_mask, edge_mask );

		for( std::size_t i = 0; i < dist_astar.size(); ++i )
			EXPECT_NEAR( dist_astar[i], dist_dsp[i], 1e-5f );
	}

} // namespace

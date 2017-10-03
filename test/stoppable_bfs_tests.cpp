//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#include <stk/geometry/geometry_kernel.hpp>
#include <stk/graph/stoppable_breadth_first_search.hpp>
#include <boost/graph/adjacency_list.hpp>

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

// visitor that terminates when we find the goal
template <class Vertex>
class bfs_goal_visitor : public boost::default_stoppable_bfs_visitor
{
public:
    bfs_goal_visitor(Vertex goal, bool& visitorTerminated)
        : m_goal(goal)
        , m_visitorTerminated(visitorTerminated)
    {}

    template <class Graph>
    bool should_stop(Vertex u, Graph& g) const
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

TEST(StoppableBFSTestSuite, stoppable_bfs_search_ThreeNodeGraphSearchingFromV1ToV2_VisitorProperlyTerminatesSearch)
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

    stoppable_breadth_first_search(g, start, boost::visitor(bfs_goal_visitor<Vertex>(goal, visitorTerminated)) );

    EXPECT_TRUE(visitorTerminated);
}

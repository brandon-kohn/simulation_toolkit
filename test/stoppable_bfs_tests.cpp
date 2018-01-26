//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

//#include <stk/geometry/geometry_kernel.hpp>
#include <geometrix/utility/assert.hpp>
#include <stk/graph/stoppable_breadth_first_search.hpp>
#include <boost/graph/adjacency_list.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

//using namespace stk;

namespace {
    enum class VertexType
    {
            Target
    };

    enum class EdgeType
    {
            Virtual
    };

    struct VertexProperties
    {
        VertexProperties(VertexType t = VertexType::Target)
            : type(t)
        {}

        VertexType type;
    };

    struct EdgeProperties
    {
        EdgeProperties(EdgeType t = EdgeType::Virtual)
            : type(t)
        {}

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
    auto v1 = boost::add_vertex(VertexProperties(VertexType::Target), g);
    auto v2 = boost::add_vertex(VertexProperties(VertexType::Target), g);
    auto v3 = boost::add_vertex(VertexProperties(VertexType::Target), g);
    {
        EdgeProperties props = { EdgeType::Virtual };
        Edge e; bool b;
        boost::tie(e, b) = boost::add_edge(v1, v2, props, g);
    }
    {
        EdgeProperties props = { EdgeType::Virtual };
        Edge e; bool b;
        boost::tie(e, b) = boost::add_edge(v2, v3, props, g);
    }
    auto start = v1;
    auto goal = v2;

    std::vector<Vertex> preds(num_vertices(g));

    bool visitorTerminated = false;

    stoppable_breadth_first_search(g, start, boost::visitor(bfs_goal_visitor<Vertex>(goal, visitorTerminated)) );

    EXPECT_TRUE(visitorTerminated);
}

TEST(StoppableBFSTestSuite, stoppable_bfs_search_using_make_stoppable_bfs_visitor)
{
	using namespace boost;

	Graph g;
	auto v1 = boost::add_vertex(VertexProperties(VertexType::Target), g);
	auto v2 = boost::add_vertex(VertexProperties(VertexType::Target), g);
	auto v3 = boost::add_vertex(VertexProperties(VertexType::Target), g);
	auto v4 = boost::add_vertex(VertexProperties(VertexType::Target), g);

	Edge e; bool b;
	auto props = EdgeProperties{ EdgeType::Virtual };
	boost::tie(e, b) = boost::add_edge(v1, v2, props, g);
	boost::tie(e, b) = boost::add_edge(v2, v3, props, g);
	boost::tie(e, b) = boost::add_edge(v3, v4, props, g);
	auto start = v1;
	auto goal = v2;

	std::vector<Vertex> preds(num_vertices(g), (std::numeric_limits<Vertex>::max)());			
	stoppable_breadth_first_search(g, start, boost::visitor(boost::make_stoppable_bfs_visitor(std::make_pair(record_predecessors(&preds[0], boost::on_tree_edge()), stop_at_goal(goal, boost::on_should_stop())))));

	bool visitorTerminated = preds[v3] == (std::numeric_limits<Vertex>::max)();
	EXPECT_TRUE(visitorTerminated);

	stoppable_breadth_first_search(g, start, boost::visitor(boost::make_stoppable_bfs_visitor(record_predecessors(&preds[0], boost::on_tree_edge()))));
	visitorTerminated = preds[v3] == (std::numeric_limits<Vertex>::max)();
	EXPECT_FALSE(visitorTerminated);
}

namespace {

	// visitor that terminates when we find the goal
	template <typename Vertex>
	class bfs_distance_visitor : public boost::default_stoppable_bfs_visitor
	{
	public:
		bfs_distance_visitor(Vertex source, std::size_t maxDistance, std::vector<std::size_t>& dmap, std::set<Vertex>& pool)
			: m_source(source)
			, m_maxDistance(maxDistance)
			, m_d(dmap)
			, m_pool(pool)
		{}

		template<typename Graph>
		void discover_vertex(Vertex u, Graph& g)
		{
			if (u != m_source && m_d[u] <= m_maxDistance)
				m_pool.insert(u);				
		}

		template<typename Graph>
		void finish_vertex(Vertex u, Graph& g)
		{
			if (m_d[u] > m_maxDistance)
				m_stop = true;
		}

		template <typename Edge, typename Graph>
		void tree_edge(Edge e, Graph& g)
		{
			auto u = boost::source(e, g);
			auto v = boost::target(e, g);
			m_d[v] = m_d[u] + 1;
		}

		template <class Graph>
		bool should_stop(Vertex u, Graph& g) const
		{
			return m_stop;
		}

	private:

		Vertex m_source;
		bool m_stop{ false };
		std::size_t m_maxDistance;
		std::vector<std::size_t>& m_d;
		std::set<Vertex>& m_pool;
	};

}//! anonymous namespace;

TEST(StoppableBFSTestSuite, stoppable_bfs_search_distance_visit)
{
	using namespace boost;
	using namespace ::testing;

	Graph g;

	std::vector<Vertex> vertices;

	for(auto i = 0; i < 18; ++i)
		vertices.emplace_back(boost::add_vertex(VertexProperties(VertexType::Target), g));

	auto props = EdgeProperties{ EdgeType::Virtual };
	
	std::vector<int> v0 = { 0, 1, 2, 3, 4 };
	std::vector<int> v1 = { 4, 5, 6, 7, 8 };
	std::vector<int> v2 = { 8, 9, 10, 11, 12 };
	std::vector<int> v3 = { 12, 13, 14, 15, 16 };

	for(auto i = 0; i < 16; i += 4)
		for (auto j = 1; j < 5; ++j)
			boost::add_edge(vertices[i], vertices[i+j], props, g);
		
	//! Add another edge outside the pattern as a wrinkle.
	boost::add_edge(vertices[5], vertices[17], props, g);

	auto start = vertices[0];
	
	std::vector<std::size_t> d(num_vertices(g), (std::numeric_limits<std::size_t>::max)());
	d[start] = 0;
	std::set<Vertex> pool;
	stoppable_breadth_first_search(g, start, boost::visitor(bfs_distance_visitor<Vertex>(start, 3, d, pool)));

	//! Pool contains all items within 3 edges.
	EXPECT_EQ(pool, std::set<Vertex>({ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 17 }));

}

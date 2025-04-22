//! Copyright © 2017
//! Brandon Kohn
//!
//! Distributed under the Boost Software License, Version 1.0. (See
//! accompanying file LICENSE_1_0.txt or copy at
//! http://www.boost.org/LICENSE_1_0.txt)

#include <stk/geometry/geometry_kernel.hpp>
#include <stk/graph/temporary_vertex_graph_adaptor.hpp>
#include <geometrix/algorithm/euclidean_distance.hpp>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/compressed_sparse_row_graph.hpp>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <vector>
#include <tuple>

//----------------------------------------------------------------
// Definitions used in tests (enums, property structs, etc.)
//----------------------------------------------------------------

enum class VertexType
{
	Obstacle,
	Target
};

enum class EdgeType
{
	Real,
	Virtual
};

using stk::point2;

struct VertexProperties
{
	VertexProperties( const point2& p = point2( 0 * boost::units::si::meters, 0 * boost::units::si::meters ),
		bool                        concave = false,
		VertexType                  t = VertexType::Obstacle )
		: position( p )
		, isConcave( concave )
		, type( t )
	{}

	point2     position;
	bool       isConcave{ false };
	VertexType type;
};

struct EdgeProperties
{
	EdgeProperties( double w = 0.0, EdgeType t = EdgeType::Virtual )
		: weight( w )
		, type( t )
	{}

	double   weight;
	EdgeType type;
};

//----------------------------------------------------------------
// Define two underlying graph types.
//----------------------------------------------------------------

using AL_Graph = boost::adjacency_list<
	boost::vecS,
	boost::vecS,
	boost::directedS,
	VertexProperties,
	EdgeProperties>;

using CSR_Graph = boost::compressed_sparse_row_graph<
	boost::directedS,
	VertexProperties,
	EdgeProperties>;

//----------------------------------------------------------------
// GraphBuilder declaration & specializations
//
// The builder takes two containers:
//   - A vector of VertexProperties (inserted in the intended order)
//   - A vector of tuples: (source index, target index, EdgeProperties)
// and returns a constructed Graph instance.
//----------------------------------------------------------------

template <typename Graph>
struct GraphBuilder;

// Builder for adjacency_list
template <>
struct GraphBuilder<AL_Graph>
{
	static AL_Graph build(
		const std::vector<VertexProperties>&                                     vertex_props,
		const std::vector<std::tuple<std::size_t, std::size_t, EdgeProperties>>& edge_data )
	{
		AL_Graph g;
		// Add vertices in order.
		for( const auto& vp : vertex_props )
		{
			boost::add_vertex( vp, g );
		}
		// Add edges using indices.
		for( const auto& t : edge_data )
		{
			std::size_t    u, v;
			EdgeProperties ep;
			std::tie( u, v, ep ) = t;
			boost::add_edge( u, v, ep, g );
		}
		return g;
	}
};

// Builder for compressed_sparse_row_graph
template <>
struct GraphBuilder<CSR_Graph>
{
	static CSR_Graph build(
		const std::vector<VertexProperties>&                                     vertex_props,
		const std::vector<std::tuple<std::size_t, std::size_t, EdgeProperties>>& edge_data )
	{
		// Convert the edge tuples into an edge list and associated properties.
		std::vector<std::pair<std::size_t, std::size_t>> edge_list;
		std::vector<EdgeProperties>                      edge_properties;
		edge_list.reserve( edge_data.size() );
		edge_properties.reserve( edge_data.size() );

		for( const auto& t : edge_data )
		{
			std::size_t    u, v;
			EdgeProperties ep;
			std::tie( u, v, ep ) = t;
			edge_list.emplace_back( u, v );
			edge_properties.push_back( ep );
		}

		// Construct the CSR graph.
		// The constructor parameters:
		//   * boost::edges_are_sorted: a tag that the edge list is sorted.
		//   * number of vertices,
		//   * begin and end iterators for the edge list,
		//   * begin iterator for edge properties,
		//   * begin and end iterators for vertex properties.
		// (If your edges are not sorted, you might need to sort or use a different constructor.)
		auto ng = CSR_Graph(
			boost::edges_are_unsorted_multi_pass,
			edge_list.begin(),
			edge_list.end(),
			edge_properties.begin(),
			vertex_props.size() );

		//! Add vertex properties to the graph.
		for( auto i = std::size_t{}; i < vertex_props.size(); ++i )
			boost::put( boost::vertex_bundle, ng, i, vertex_props[i] );

		return ng;
	}
};

//----------------------------------------------------------------
// GoogleTest typed test fixture using a templated Graph.
//----------------------------------------------------------------

template <typename GraphType>
class DynamicNavigationTestSuite : public ::testing::Test
{
public:
	using Graph = GraphType;
	using Vertex = typename Graph::vertex_descriptor;
	using Edge = typename Graph::edge_descriptor;
};

typedef ::testing::Types<AL_Graph, CSR_Graph> GraphTypes;
TYPED_TEST_SUITE( DynamicNavigationTestSuite, GraphTypes );

//----------------------------------------------------------------
// Now each test uses the builder to create an initial immutable graph
// and then creates the adaptor (SUT) which allows additional vertices
// and edges to be added.
//----------------------------------------------------------------

TYPED_TEST( DynamicNavigationTestSuite, adjacency_iterator_iteration_over_base )
{
	using Graph = typename TestFixture::Graph;

	// Build the base graph with two vertices and one edge.
	auto                          p1 = point2{ 0. * boost::units::si::meters, 0. * boost::units::si::meters };
	auto                          p2 = point2{ 0. * boost::units::si::meters, 1. * boost::units::si::meters };
	std::vector<VertexProperties> vertices{
		VertexProperties( p1, true, VertexType::Target ),
		VertexProperties( p2, true, VertexType::Target )
	};

	double                                                            weight = geometrix::point_point_distance( p1, p2 ).value();
	std::vector<std::tuple<std::size_t, std::size_t, EdgeProperties>> edges{
		std::make_tuple( 0, 1, EdgeProperties( weight, EdgeType::Virtual ) )
	};

	Graph g = GraphBuilder<Graph>::build( vertices, edges );

	// Create the SUT with an extra vertex (p3) and additional connectivity.
	auto                                              p3 = point2{ 3. * boost::units::si::meters, 1. * boost::units::si::meters };
	stk::graph::temporary_vertex_graph_adaptor<Graph> ag(
		g,
		VertexProperties( p3, true, VertexType::Target ),
		{ { 0, EdgeProperties( weight, EdgeType::Virtual ) },
			{ 1, EdgeProperties( weight, EdgeType::Virtual ) } } );

	auto [ai, aend] = boost::out_edges( 0, ag );
	EXPECT_EQ( boost::target( *ai++, ag ), 1 );
	EXPECT_EQ( ai, aend );
}

TYPED_TEST( DynamicNavigationTestSuite, adjacency_iterator_iteration_over_new )
{
	using Graph = typename TestFixture::Graph;

	// Build the base graph.
	auto                          p1 = point2{ 0. * boost::units::si::meters, 0. * boost::units::si::meters };
	auto                          p2 = point2{ 0. * boost::units::si::meters, 1. * boost::units::si::meters };
	std::vector<VertexProperties> vertices{
		VertexProperties( p1, true, VertexType::Target ),
		VertexProperties( p2, true, VertexType::Target )
	};

	double                                                            weight = geometrix::point_point_distance( p1, p2 ).value();
	std::vector<std::tuple<std::size_t, std::size_t, EdgeProperties>> edges{
		std::make_tuple( 0, 1, EdgeProperties( weight, EdgeType::Virtual ) )
	};

	Graph g = GraphBuilder<Graph>::build( vertices, edges );

	auto                                              p3 = point2{ 3. * boost::units::si::meters, 1. * boost::units::si::meters };
	stk::graph::temporary_vertex_graph_adaptor<Graph> ag(
		g,
		VertexProperties( p3, true, VertexType::Target ),
		{ { 0, EdgeProperties( weight, EdgeType::Virtual ) },
			{ 1, EdgeProperties( weight, EdgeType::Virtual ) } } );

	// Obtain the new vertex index.
	auto v3 = ag.new_indices()[0];
	auto [ai, aend] = boost::out_edges( v3, ag );
	EXPECT_EQ( boost::target( *ai++, ag ), 0 );
	EXPECT_EQ( boost::target( *ai++, ag ), 1 );
	EXPECT_EQ( ai, aend );
}

TYPED_TEST( DynamicNavigationTestSuite, vertex_iterator_over_graph )
{
	using Graph = typename TestFixture::Graph;

	// Build graph with two vertices and one edge.
	auto                          p1 = point2{ 0. * boost::units::si::meters, 0. * boost::units::si::meters };
	auto                          p2 = point2{ 0. * boost::units::si::meters, 1. * boost::units::si::meters };
	std::vector<VertexProperties> vertices{
		VertexProperties( p1, true, VertexType::Target ),
		VertexProperties( p2, true, VertexType::Target )
	};

	double                                                            weight = geometrix::point_point_distance( p1, p2 ).value();
	std::vector<std::tuple<std::size_t, std::size_t, EdgeProperties>> edges{
		std::make_tuple( 0, 1, EdgeProperties( weight, EdgeType::Virtual ) )
	};

	Graph g = GraphBuilder<Graph>::build( vertices, edges );

	auto                                              p3 = point2{ 3. * boost::units::si::meters, 1. * boost::units::si::meters };
	stk::graph::temporary_vertex_graph_adaptor<Graph> ag(
		g,
		VertexProperties( p3, true, VertexType::Target ),
		{ { 0, EdgeProperties( weight, EdgeType::Virtual ) },
			{ 1, EdgeProperties( weight, EdgeType::Virtual ) } } );

	auto [vi, vend] = boost::vertices( ag );
	// Expect the two original vertices and the new vertex.
	EXPECT_EQ( *vi++, 0 );
	EXPECT_EQ( *vi++, 1 );
	EXPECT_EQ( *vi++, 2 );
	EXPECT_EQ( vi, vend );
}

TYPED_TEST( DynamicNavigationTestSuite, edge_iterator_over_graph )
{
	using Graph = typename TestFixture::Graph;

	// Build the base graph.
	auto                          p1 = point2{ 0. * boost::units::si::meters, 0. * boost::units::si::meters };
	auto                          p2 = point2{ 0. * boost::units::si::meters, 1. * boost::units::si::meters };
	std::vector<VertexProperties> vertices{
		VertexProperties( p1, true, VertexType::Target ),
		VertexProperties( p2, true, VertexType::Target )
	};

	double                                                            weight = geometrix::point_point_distance( p1, p2 ).value();
	std::vector<std::tuple<std::size_t, std::size_t, EdgeProperties>> edges{
		std::make_tuple( 0, 1, EdgeProperties( weight, EdgeType::Virtual ) )
	};

	Graph g = GraphBuilder<Graph>::build( vertices, edges );

	// For the adaptor, add new edges keyed by the base graph vertices.
	auto                                              p3 = point2{ 3. * boost::units::si::meters, 1. * boost::units::si::meters };
	stk::graph::temporary_vertex_graph_adaptor<Graph> ag(
		g,
		VertexProperties( p3, true, VertexType::Target ),
		{ { 0, EdgeProperties( weight, EdgeType::Virtual ) },
			{ 1, EdgeProperties( weight, EdgeType::Virtual ) } } );
	auto v3 = ag.new_indices()[0];

	auto [ei, eend] = boost::edges( ag );
	EXPECT_EQ( boost::source( *ei, ag ), 0 );
	EXPECT_EQ( boost::target( *ei, ag ), 1 );
	++ei;
	EXPECT_EQ( boost::source( *ei, ag ), v3 );
	EXPECT_EQ( boost::target( *ei, ag ), 0 );
	++ei;
	EXPECT_EQ( boost::source( *ei, ag ), v3 );
	EXPECT_EQ( boost::target( *ei, ag ), 1 );
	++ei;
	EXPECT_EQ( ei, eend );
}

TYPED_TEST( DynamicNavigationTestSuite, edge_properties )
{
	using Graph = typename TestFixture::Graph;

	// Build base graph.
	auto                          p1 = point2{ 0. * boost::units::si::meters, 0. * boost::units::si::meters };
	auto                          p2 = point2{ 0. * boost::units::si::meters, 1. * boost::units::si::meters };
	std::vector<VertexProperties> vertices{
		VertexProperties( p1, true, VertexType::Target ),
		VertexProperties( p2, true, VertexType::Target )
	};

	double                                                            weight = geometrix::point_point_distance( p1, p2 ).value();
	std::vector<std::tuple<std::size_t, std::size_t, EdgeProperties>> edges{
		std::make_tuple( 0, 1, EdgeProperties( weight, EdgeType::Virtual ) )
	};

	Graph g = GraphBuilder<Graph>::build( vertices, edges );

	auto                                              p3 = point2{ 3. * boost::units::si::meters, 1. * boost::units::si::meters };
	stk::graph::temporary_vertex_graph_adaptor<Graph> ag(
		g,
		VertexProperties( p3, true, VertexType::Target ),
		{ { 0, EdgeProperties( weight, EdgeType::Virtual ) },
			{ 1, EdgeProperties( weight, EdgeType::Virtual ) } } );

	auto w = ag[boost::edge( 0, 1, ag ).first].weight;
	EXPECT_EQ( weight, w );

	w = boost::get( &EdgeProperties::weight, ag, boost::edge( 0, 1, ag ).first );
	EXPECT_EQ( weight, w );
}

TYPED_TEST( DynamicNavigationTestSuite, vertex_properties )
{
	using Graph = typename TestFixture::Graph;

	// Build the base graph.
	auto                          p1 = point2{ 0. * boost::units::si::meters, 0. * boost::units::si::meters };
	auto                          p2 = point2{ 0. * boost::units::si::meters, 1. * boost::units::si::meters };
	std::vector<VertexProperties> vertices{
		VertexProperties( p1, true, VertexType::Target ),
		VertexProperties( p2, true, VertexType::Target )
	};

	double                                                            weight = geometrix::point_point_distance( p1, p2 ).value();
	std::vector<std::tuple<std::size_t, std::size_t, EdgeProperties>> edges{
		std::make_tuple( 0, 1, EdgeProperties( weight, EdgeType::Virtual ) )
	};

	Graph g = GraphBuilder<Graph>::build( vertices, edges );

	// Use a new vertex with a different type.
	auto                                              p3 = point2{ 3. * boost::units::si::meters, 1. * boost::units::si::meters };
	stk::graph::temporary_vertex_graph_adaptor<Graph> ag(
		g,
		VertexProperties( p3, true, VertexType::Obstacle ),
		{ { 0, EdgeProperties( weight, EdgeType::Virtual ) },
			{ 1, EdgeProperties( weight, EdgeType::Virtual ) } } );
	auto v3 = ag.new_indices()[0];

	auto t = boost::get( &VertexProperties::type, ag, 0 );
	EXPECT_EQ( VertexType::Target, t );

	t = boost::get( &VertexProperties::type, ag, v3 );
	EXPECT_EQ( VertexType::Obstacle, t );
}

TYPED_TEST( DynamicNavigationTestSuite, add_edge_OldVertex )
{
	using Graph = typename TestFixture::Graph;

	// Build the base graph.
	auto                          p1 = point2{ 0. * boost::units::si::meters, 0. * boost::units::si::meters };
	auto                          p2 = point2{ 0. * boost::units::si::meters, 1. * boost::units::si::meters };
	std::vector<VertexProperties> vertices{
		VertexProperties( p1, true, VertexType::Target ),
		VertexProperties( p2, true, VertexType::Target )
	};

	double                                                            weight = geometrix::point_point_distance( p1, p2 ).value();
	std::vector<std::tuple<std::size_t, std::size_t, EdgeProperties>> edges{
		std::make_tuple( 0, 1, EdgeProperties( weight, EdgeType::Virtual ) )
	};

	Graph g = GraphBuilder<Graph>::build( vertices, edges );

	auto                                              p3 = point2{ 3. * boost::units::si::meters, 1. * boost::units::si::meters };
	stk::graph::temporary_vertex_graph_adaptor<Graph> ag(
		g,
		VertexProperties( p3, true, VertexType::Obstacle ),
		{ { 0, EdgeProperties( weight, EdgeType::Virtual ) },
			{ 1, EdgeProperties( weight, EdgeType::Virtual ) } } );
	using vertex_descriptor = typename Graph::vertex_descriptor;
	auto v3 = vertex_descriptor( ag.new_indices()[0] );

	auto edge_pair = boost::add_edge( 0, v3, EdgeProperties{ 66.0, EdgeType::Real }, ag );
	ASSERT_TRUE( edge_pair.second );

	auto eType = boost::get( &EdgeProperties::type, ag, edge_pair.first );
	EXPECT_EQ( EdgeType::Real, eType );
	EXPECT_EQ( eType, ag[edge_pair.first].type );

	auto eWeight = boost::get( &EdgeProperties::weight, ag, edge_pair.first );
	EXPECT_DOUBLE_EQ( 66.0, eWeight );
	EXPECT_EQ( eWeight, ag[edge_pair.first].weight );
}

TYPED_TEST( DynamicNavigationTestSuite, num_vertices_TwoOldTwoNew_Returns4 )
{
	using Graph = typename TestFixture::Graph;

	// Build base graph with two vertices and one edge.
	auto                          p1 = point2{ 0. * boost::units::si::meters, 0. * boost::units::si::meters };
	auto                          p2 = point2{ 0. * boost::units::si::meters, 1. * boost::units::si::meters };
	std::vector<VertexProperties> vertices{
		VertexProperties( p1, true, VertexType::Target ),
		VertexProperties( p2, true, VertexType::Target )
	};

	double                                                            weight = geometrix::point_point_distance( p1, p2 ).value();
	std::vector<std::tuple<std::size_t, std::size_t, EdgeProperties>> edges{
		std::make_tuple( 0, 1, EdgeProperties( weight, EdgeType::Virtual ) )
	};

	Graph g = GraphBuilder<Graph>::build( vertices, edges );

	// Wrap the base graph; then add two new vertices using the adaptor.
	stk::graph::temporary_vertex_graph_adaptor<Graph> ag( g );

	auto p3 = point2{ 3. * boost::units::si::meters, 1. * boost::units::si::meters };
	ag.add_vertex( VertexProperties( p3, true, VertexType::Obstacle ) );

	auto p4 = point2{ 4. * boost::units::si::meters, 1. * boost::units::si::meters };
	ag.add_vertex( VertexProperties( p4, true, VertexType::Obstacle ) );

	auto result = boost::num_vertices( ag );
	EXPECT_EQ( 4, result );
}

#include <boost/graph/compressed_sparse_row_graph.hpp>
#include <boost/graph/filtered_graph.hpp>

// Dummy vertex and edge property types for testing
struct DummyVertex
{
	int id;
};

struct DummyEdge
{
	double weight;
};

// Vertex predicate: keep only vertices with even id
template <typename G>
struct EvenVertexPred
{
	const G* graph;
	EvenVertexPred() = default; // Default constructor for filtered graph
	EvenVertexPred( const G& g )
		: graph( &g )
	{}

	bool operator()( typename boost::graph_traits<G>::vertex_descriptor v ) const
	{
		return ( ( *graph )[v].id % 2 ) == 0;
	}
};

// Edge predicate: keep only edges whose source and target satisfy the vertex predicate
template <typename G>
struct EdgePred
{
	const G*    graph;
	EdgePred() = default; // Default constructor for filtered graph
	EdgePred( const G& g )
		: graph( &g )
	{}

	bool operator()( typename boost::graph_traits<G>::edge_descriptor e ) const
	{
		return ( *graph )[e].weight > 1.0;
	}
};

TEST( TemporaryAdaptorCSRFilterTest, BasicFilteredBehavior )
{
	using CSR = boost::compressed_sparse_row_graph<
		boost::directedS,
		DummyVertex,
		DummyEdge>;

	static_assert( std::is_same_v<CSR::vertex_descriptor, std::size_t>, "Vertex descriptor should be size_t" );

	// Build base CSR graph: 4 vertices (id=0..3) in a ring
	std::vector<std::size_t> sources = { 0, 1, 2, 3 };
	std::vector<std::size_t> targets = { 2, 2, 3, 0 };
	std::vector<DummyEdge>   eprops = { { 1.6 }, { 0.5 }, { 1.6 }, { 1.5 } };

	CSR csr(
		boost::construct_inplace_from_sources_and_targets,
		sources,
		targets,
		eprops,
		eprops.size() );

	csr[0].id = 0;
	csr[1].id = 1;
	csr[2].id = 2;
	csr[3].id = 3;

	// Wrap in temporary vertex adaptor
	stk::graph::temporary_vertex_graph_adaptor<CSR> ag( csr );

	using TVG = stk::graph::temporary_vertex_graph_adaptor<CSR>;

	// Define filtered graph: even_numbered vertices only
	EvenVertexPred<TVG> vpred( ag );
	EdgePred<TVG>       epred( ag );
	using Filtered = boost::filtered_graph<TVG, EdgePred<TVG>, EvenVertexPred<TVG>>;
	Filtered fg( ag, epred, vpred );

	auto             verts = boost::vertices( fg );
	std::vector<int> degs;
	for( auto it = verts.first; it != verts.second; ++it )
		degs.push_back( boost::out_degree( *it, fg ) );
	EXPECT_EQ( degs.size(), 2 );
	// Vertex 0 has out_edge to 2: degree 1
	// Vertex 2 has out_edge to ? (3 filtered out): degree 0
	EXPECT_EQ( degs[0], 1 );
	EXPECT_EQ( degs[1], 0 );
}

TYPED_TEST( DynamicNavigationTestSuite, vertex_property_accessors )
{
	using Graph = typename TestFixture::Graph;

	// Build base graph.
	auto                          p1 = point2{ 0. * boost::units::si::meters, 0. * boost::units::si::meters };
	std::vector<VertexProperties> vertices{
		VertexProperties( p1, false, VertexType::Target )
	};
	std::vector<std::tuple<std::size_t, std::size_t, EdgeProperties>> edges; // empty

	Graph g = GraphBuilder<Graph>::build( vertices, edges );

	// Wrap, and add one new vertex with a different type.
	auto                                              p2 = point2{ 1. * boost::units::si::meters, 0. * boost::units::si::meters };
	stk::graph::temporary_vertex_graph_adaptor<Graph> ag(
		g,
		VertexProperties( p2, true, VertexType::Obstacle ),
		{} );
	auto new_v = ag.new_indices()[0];

	// old vertex 0 
	// bundled
	const auto& vp0_bundled = ag[0];
	EXPECT_EQ( vp0_bundled.type, VertexType::Target );
	// property_map
	auto vp0_map = boost::get( &VertexProperties::type, ag, 0 );
	EXPECT_EQ( vp0_map, VertexType::Target );

	// new vertex
	// bundled
	const auto& vpn_bundled = ag[new_v];
	EXPECT_EQ( vpn_bundled.type, VertexType::Obstacle );
	// property_map
	auto vpn_map = boost::get( &VertexProperties::type, ag, new_v );
	EXPECT_EQ( vpn_map, VertexType::Obstacle );
}

TYPED_TEST( DynamicNavigationTestSuite, edge_property_accessors )
{
	using Graph = typename TestFixture::Graph;

	// Build a base graph with two vertices and one edge.
	auto                                                              p1 = point2{ 0. * boost::units::si::meters, 0. * boost::units::si::meters };
	auto                                                              p2 = point2{ 1. * boost::units::si::meters, 0. * boost::units::si::meters };
	std::vector<VertexProperties>                                     vertices{ VertexProperties( p1 ), VertexProperties( p2 ) };
	double                                                            weight = geometrix::point_point_distance( p1, p2 ).value();
	std::vector<std::tuple<std::size_t, std::size_t, EdgeProperties>> edges{
		std::make_tuple( 0, 1, EdgeProperties( weight, EdgeType::Virtual ) )
	};
	Graph g = GraphBuilder<Graph>::build( vertices, edges );

	// Wrap and add one new edge from old vertex 0 to old vertex 1
	stk::graph::temporary_vertex_graph_adaptor<Graph> ag( g );
	auto                                              epr = EdgeProperties{ 3.14, EdgeType::Real };
	auto                                              edge_pair = boost::add_edge( 0, 1, epr, ag );
	ASSERT_TRUE( edge_pair.second );
	auto ed = edge_pair.first;

	// bundled
	const auto& ep_bundled = ag[ed];
	EXPECT_DOUBLE_EQ( ep_bundled.weight, 3.14 );
	EXPECT_EQ( ep_bundled.type, EdgeType::Real );

	// property_map
	auto w_map = boost::get( &EdgeProperties::weight, ag, ed );
	auto t_map = boost::get( &EdgeProperties::type, ag, ed );
	EXPECT_DOUBLE_EQ( w_map, 3.14 );
	EXPECT_EQ( t_map, EdgeType::Real );
}

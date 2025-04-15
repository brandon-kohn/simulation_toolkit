//
//! Copyright © 2017-2025
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/iterator/iterator_facade.hpp>
#include <boost/static_assert.hpp>

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/properties.hpp>
#include <boost/graph/graph_concepts.hpp>
#include <boost/graph/graph_utility.hpp>
#include <boost/graph/compressed_sparse_row_graph.hpp>
#include <boost/graph/adjacency_list.hpp>

#include <boost/range/iterator_range.hpp>
#include <boost/range/irange.hpp>

#include <geometrix/utility/assert.hpp>

#include <vector>
#include <unordered_map>
#include <cassert>
#include <iterator>
#include <type_traits>
#include <variant>

//------------------------------------------------------
// In namespace stk::graph
//------------------------------------------------------
namespace stk::graph {

	//! ==========================================================================
	//! Generic Directed Graph Check
	//! ==========================================================================
	template <typename Graph>
	struct is_directed_graph : std::is_same<typename boost::graph_traits<Graph>::directed_category, boost::directed_tag>
	{};

	template <typename Graph>
	struct temporary_vertex_graph_adaptor;

	template <typename Graph, typename Enable = void>
	struct vertex_property_type_of_impl
	{
		static_assert( false, "define vertex_property_type_of_impl for the specified graph type" );
	};

	template <typename ... Params>
	struct vertex_property_type_of_impl<boost::adjacency_list<Params...>>
	{
		using type = typename boost::adjacency_list<Params...>::vertex_property_type;
	};

	template <typename ... Params>
	struct vertex_property_type_of_impl<boost::compressed_sparse_row_graph<Params...>>
	{
		using type = typename boost::compressed_sparse_row_graph<Params...>::vertex_bundled;
	};

	// Primary template: fallback if Graph does not have an edge_property_type typedef.
	template <typename Graph, typename Enable = void>
	struct edge_property_type_of_impl
	{
		static_assert( false, "define edge_property_type_of_impl for the specified graph type" );
	};

	template <typename ... Params>
	struct edge_property_type_of_impl<boost::adjacency_list<Params...>>
	{
		using type = typename boost::adjacency_list<Params...>::edge_property_type;
	};

	template <typename ... Params>
	struct edge_property_type_of_impl<boost::compressed_sparse_row_graph<Params...>>
	{
		using type = typename boost::compressed_sparse_row_graph<Params...>::edge_bundled;
	};

	//------------------------------------------------------
	//! vertex_property_type_of_impl and vertex_property_type_of
	//------------------------------------------------------
	template <typename Graph>
	struct vertex_property_type_of
	{
		typedef typename vertex_property_type_of_impl<typename std::decay<Graph>::type>::type type;
	};

	//------------------------------------------------------
	//! edge_property_type_of_impl and edge_property_type_of
	//------------------------------------------------------
	template <typename Graph>
	struct edge_property_type_of
	{
		typedef typename edge_property_type_of_impl<typename std::decay<Graph>::type>::type type;
	};

	//------------------------------------------------------
	//! fused_vertex_iterator: Iterates over the union of (a) the original vertices
	//! (as given by boost::vertices) and (b) indices for new vertices.
	//------------------------------------------------------
	template <typename Graph>
	class fused_vertex_iterator : public boost::iterator_facade<fused_vertex_iterator<Graph>, typename boost::graph_traits<Graph>::vertex_descriptor, boost::forward_traversal_tag, typename boost::graph_traits<Graph>::vertex_descriptor>
	{
	public:
		typedef temporary_vertex_graph_adaptor<Graph>                      adaptor_type;
		typedef typename adaptor_type::vertex_descriptor                   vertex_descriptor;
		typedef typename adaptor_type::edge_descriptor                     edge_descriptor;
		typedef typename boost::graph_traits<Graph>::vertex_iterator       original_iterator;
		typedef typename boost::integer_range<std::size_t>::const_iterator new_iterator;

		fused_vertex_iterator()
			: m_adaptor( nullptr )
		{}

		fused_vertex_iterator( const adaptor_type* adaptor,
			original_iterator                      orig_it,
			original_iterator                      orig_end,
			new_iterator                           new_it,
			new_iterator                           new_end )
			: m_adaptor( adaptor )
			, m_orig_it( orig_it )
			, m_orig_end( orig_end )
			, m_new_it( new_it )
			, m_new_end( new_end )
		{}

	private:
		friend class boost::iterator_core_access;
		vertex_descriptor dereference() const
		{
			if( m_orig_it != m_orig_end )
				return m_adaptor->make_original_descriptor( *m_orig_it );
			else
				return m_adaptor->make_new_descriptor( *m_new_it );
		}
		void increment()
		{
			if( m_orig_it != m_orig_end )
				++m_orig_it;
			else
			{
				GEOMETRIX_ASSERT( m_new_it != m_new_end );
				++m_new_it;
			}
		}
		bool equal( fused_vertex_iterator const& other ) const
		{
			return ( m_orig_it == other.m_orig_it ) && ( m_new_it == other.m_new_it );
		}

		const adaptor_type* m_adaptor;
		original_iterator   m_orig_it, m_orig_end;
		new_iterator        m_new_it, m_new_end;
	};

	//------------------------------------------------------
	//! fused_out_edge_iterator: Iterates over a vertex's out-edges by fusing
	//! the underlying out edges from the original graph with the extra edges stored in the adaptor.
	//------------------------------------------------------
	template <typename Graph>
	class fused_out_edge_iterator : public boost::iterator_facade<fused_out_edge_iterator<Graph>, typename temporary_vertex_graph_adaptor<Graph>::edge_descriptor, boost::forward_traversal_tag, typename temporary_vertex_graph_adaptor<Graph>::edge_descriptor>
	{
	public:
		typedef temporary_vertex_graph_adaptor<Graph>                                                                  adaptor_type;
		typedef typename adaptor_type::edge_descriptor                                                                 edge_descriptor;
		typedef typename adaptor_type::vertex_descriptor                                                               vertex_descriptor;
		typedef typename boost::graph_traits<Graph>::out_edge_iterator                                                 base_iterator;
		typedef typename std::vector<std::pair<vertex_descriptor, typename adaptor_type::edge_property_type>>::const_iterator extra_iterator;

		fused_out_edge_iterator()
			: m_adaptor( nullptr )
		{}

		fused_out_edge_iterator( const adaptor_type* adaptor,
			vertex_descriptor                        source,
			base_iterator                            base_it,
			base_iterator                            base_end,
			extra_iterator                           extra_it,
			extra_iterator                           extra_end )
			: m_adaptor( adaptor )
			, m_source( source )
			, m_base_it( base_it )
			, m_base_end( base_end )
			, m_extra_it( extra_it )
			, m_extra_end( extra_end )
		{}

	private:
		friend class boost::iterator_core_access;
		edge_descriptor dereference() const
		{
			if( m_base_it != m_base_end )
			{
				vertex_descriptor tgt = m_adaptor->make_original_descriptor( boost::target( *m_base_it, m_adaptor->graph() ) );
				// Use boost::get to obtain the edge property.
				const typename adaptor_type::edge_property_type* prop = &boost::get( boost::edge_all, m_adaptor->graph() )[*m_base_it];
				return m_adaptor->make_edge_descriptor( m_source, tgt, prop );
			}
			else
			{
				return m_adaptor->make_edge_descriptor( m_source, m_extra_it->first, &m_extra_it->second );
			}
		}
		void increment()
		{
			if( m_base_it != m_base_end )
				++m_base_it;
			else
			{
				GEOMETRIX_ASSERT( m_extra_it != m_extra_end );
				++m_extra_it;
			}
		}
		bool equal( fused_out_edge_iterator const& other ) const
		{
			return m_base_it == other.m_base_it && m_extra_it == other.m_extra_it;
		}

		const adaptor_type* m_adaptor;
		vertex_descriptor   m_source;
		base_iterator       m_base_it, m_base_end;
		extra_iterator      m_extra_it, m_extra_end;
	};

	//------------------------------------------------------
	//! temporary_vertex_graph_adaptor
	//! An adaptor that wraps an immutable directed graph and permits adding
	//! temporary vertices and extra edges.
	//------------------------------------------------------
	template <typename Graph>
	struct temporary_vertex_graph_adaptor
	{
		BOOST_STATIC_ASSERT( is_directed_graph<Graph>::value );

	public:
		typedef Graph                                                  graph_t;
		typedef typename boost::graph_traits<Graph>::vertex_descriptor original_vertex_descriptor;

		static constexpr bool                                            using_integral = std::is_integral<original_vertex_descriptor>::value;
		typedef typename std::conditional<using_integral,
			original_vertex_descriptor,
			std::variant<original_vertex_descriptor, std::size_t>>::type vertex_descriptor;

		typedef typename vertex_property_type_of<Graph>::type vertex_property_type;
		typedef typename edge_property_type_of<Graph>::type   edge_property_type;

		//! The unified edge_descriptor holds the unified source/target and a pointer to the underlying edge property.
		struct edge_descriptor
		{
			vertex_descriptor         source;
			vertex_descriptor         target;
			const edge_property_type* property;
			edge_descriptor()
				: property( nullptr )
			{}
			edge_descriptor( vertex_descriptor s, vertex_descriptor t, const edge_property_type* p )
				: source( s )
				, target( t )
				, property( p )
			{}
		};


		typedef fused_vertex_iterator<Graph>   vertex_iterator;
		typedef fused_out_edge_iterator<Graph> out_edge_iterator;

		//-----------------------------------------
		//! Constructors
		//-----------------------------------------
		temporary_vertex_graph_adaptor( const Graph& graph )
			: mGraph( graph )
		{}

		temporary_vertex_graph_adaptor( const Graph&                             graph,
			const vertex_property_type&                                          newV,
			const std::vector<std::pair<vertex_descriptor, edge_property_type>>& newAdjacencies )
			: mGraph( graph )
		{
			vertex_descriptor v = add_vertex( newV );
			for( auto const& adj : newAdjacencies )
				add_edge( v, adj.first, adj.second );
		}

		//-----------------------------------------
		//! Vertex Functions
		//-----------------------------------------
		std::size_t num_vertices() const
		{
			return boost::num_vertices( mGraph ) + mNewVertexProperties.size();
		}

		vertex_descriptor add_vertex( const vertex_property_type& p )
		{
			mNewVertexProperties.push_back( p );
			mNewOutEdges.emplace_back();
			return make_new_descriptor( num_vertices() - 1 );
		}

		vertex_descriptor add_vertex()
		{
			return add_vertex( vertex_property_type() );
		}

		//-----------------------------------------
		//! Edge Functions
		//-----------------------------------------
		std::pair<edge_descriptor, bool> add_edge( vertex_descriptor u, vertex_descriptor v, const edge_property_type& p )
		{
			if( is_new( u ) )
			{
				std::size_t idx = get_new_index( u );
				GEOMETRIX_ASSERT( mNewOutEdges.size() > idx );
				mNewOutEdges[idx].push_back( std::make_pair( v, p ) );
				return std::make_pair( make_edge_descriptor( u, v, &p ), true );
			}
			else
			{
				mAdaptedOriginalEdges[get_original( u )].push_back( std::make_pair( v, p ) );
				return std::make_pair( make_edge_descriptor( u, v, &p ), true );
			}
		}

		//-----------------------------------------
		//! Out-Edge Iterator Support
		//-----------------------------------------
		std::pair<out_edge_iterator, out_edge_iterator> out_edge_range( vertex_descriptor v ) const
		{
			if( is_new( v ) )
			{
				std::size_t                                            idx = get_new_index( v );
				typename boost::graph_traits<Graph>::out_edge_iterator base_it{}, base_end{};
				GEOMETRIX_ASSERT( mNewOutEdges.size() > idx );
				const auto& extra = mNewOutEdges[idx];
				return std::make_pair(
					out_edge_iterator( this, v, base_it, base_end, extra.begin(), extra.end() ),
					out_edge_iterator( this, v, base_it, base_end, extra.end(), extra.end() ) );
			}
			else
			{
				original_vertex_descriptor orig = get_original( v );
				auto                       underlying_range = boost::out_edges( orig, mGraph );
				auto                       itAdapt = mAdaptedOriginalEdges.find( orig );
				const auto&                extra = ( itAdapt != mAdaptedOriginalEdges.end() ) ? itAdapt->second : empty_extra_edges();
				return std::make_pair(
					out_edge_iterator( this, v, underlying_range.first, underlying_range.second, extra.begin(), extra.end() ),
					out_edge_iterator( this, v, underlying_range.second, underlying_range.second, extra.end(), extra.end() ) );
			}
		}

		//-----------------------------------------
		//! Vertex Iterator Support
		//-----------------------------------------
		std::pair<vertex_iterator, vertex_iterator> vertices() const
		{
			auto [orig_begin, orig_end] = boost::vertices( mGraph );
			auto newIndices = new_indices();
			return std::make_pair(
				vertex_iterator( this, orig_begin, orig_end, newIndices.begin(), newIndices.end() ),
				vertex_iterator( this, orig_end, orig_end, newIndices.end(), newIndices.end() ) );
		}
		vertex_iterator vertices_begin() const { return vertices().first; }
		vertex_iterator vertices_end() const { return vertices().second; }

		//-----------------------------------------
		//! New Vertex Indices Helper
		//-----------------------------------------
		boost::integer_range<std::size_t> new_indices() const
		{
			auto oSize = boost::num_vertices( mGraph );
			return boost::irange( oSize, oSize + mNewVertexProperties.size() );
		}

		//-----------------------------------------
		//! Unified Vertex Descriptor Helpers
		//-----------------------------------------
		vertex_descriptor make_new_descriptor( std::size_t idx ) const
		{
			if constexpr( using_integral )
				return idx;
			else
				return vertex_descriptor( idx );
		}
		vertex_descriptor make_original_descriptor( const original_vertex_descriptor& v ) const
		{
			if constexpr( using_integral )
				return v;
			else
				return vertex_descriptor( v );
		}
		bool is_new( const vertex_descriptor& v ) const
		{
			if constexpr( using_integral )
				return v >= boost::num_vertices( mGraph );
			else
				return std::holds_alternative<std::size_t>( v );
		}
		std::size_t get_new_index( const vertex_descriptor& v ) const
		{
			if constexpr( using_integral )
				return v - boost::num_vertices( mGraph );
			else
				return std::get<std::size_t>( v );
		}
		original_vertex_descriptor get_original( const vertex_descriptor& v ) const
		{
			if constexpr( using_integral )
				return v;
			else
				return std::get<original_vertex_descriptor>( v );
		}

		//-----------------------------------------
		//! Synthesize an edge_descriptor.
		//-----------------------------------------
		edge_descriptor make_edge_descriptor( vertex_descriptor u, vertex_descriptor v, const edge_property_type* p ) const
		{
			return edge_descriptor( u, v, p );
		}

		//-----------------------------------------
		//! Helper: Provide a reference to an empty extra-edge vector.
		//-----------------------------------------
		static const std::vector<std::pair<vertex_descriptor, edge_property_type>>& empty_extra_edges()
		{
			static const std::vector<std::pair<vertex_descriptor, edge_property_type>> empty;
			return empty;
		}

		//-----------------------------------------
		//! Property Access Operators
		//-----------------------------------------
		//! For vertex properties, if the vertex is original, use the underlying graph's
		//! property map (via boost::get(boost::vertex_all, ...)); for new vertices, use
		//! the stored property.
		const vertex_property_type& operator[]( const vertex_descriptor& v ) const
		{
			if( is_new( v ) )
				return mNewVertexProperties[get_new_index( v )];
			else
				return graph()[get_original( v )];
		}

		//! For unified edge descriptors, the property pointer was stored during construction.
		const edge_property_type& operator[]( const edge_descriptor& ed ) const
		{
			return *( ed.property );
		}

		//! For original edge descriptors, delegate to the underlying graph's property map.
		// (These overloads are less-used since free-function get() is provided below.)
		/*
		const typename edge_property_type& operator[]( const typename boost::graph_traits<Graph>::edge_descriptor& e ) const
		{
		    return boost::get( boost::edge_all, graph() )[e];
		}
		*/

		//-----------------------------------------
		//! Public accessor to the underlying graph.
		//-----------------------------------------
		const Graph& graph() const { return mGraph; }

	private:
		// Grant friendship to fused_out_edge_iterator so it can call graph()
		template <typename G>
		friend class fused_out_edge_iterator;

		// The underlying graph (immutable).
		const Graph& mGraph;
		// Storage for new vertices.
		std::vector<vertex_property_type> mNewVertexProperties;
		// Extra out-edges for new vertices.
		std::vector<std::vector<std::pair<vertex_descriptor, edge_property_type>>> mNewOutEdges;
		// Extra edges added for original vertices.
		typedef std::unordered_map<original_vertex_descriptor,
			std::vector<std::pair<vertex_descriptor, edge_property_type>>>
							   adapted_edges_map_type;
		adapted_edges_map_type mAdaptedOriginalEdges;
	};

} // namespace stk::graph

// --------------------------------------------------------------------------
//! Boost Graph Free Function Overloads and Traits
// --------------------------------------------------------------------------
namespace boost {

	template <typename Graph>
	struct graph_traits<stk::graph::temporary_vertex_graph_adaptor<Graph>>
	{
		typedef stk::graph::temporary_vertex_graph_adaptor<Graph> adaptor_type;
		typedef typename adaptor_type::vertex_descriptor          vertex_descriptor;
		typedef typename adaptor_type::edge_descriptor            edge_descriptor;
		typedef boost::directed_tag                               directed_category;
		typedef allow_parallel_edge_tag                           edge_parallel_category;
		typedef boost::incidence_graph_tag                        traversal_category;
		typedef std::size_t                                       vertices_size_type;
		typedef std::size_t                                       edges_size_type;
		typedef std::size_t                                       degree_size_type;

		typedef typename adaptor_type::vertex_iterator   vertex_iterator;
		typedef typename adaptor_type::out_edge_iterator out_edge_iterator;
	};

	template <typename Graph>
	inline std::size_t num_vertices( const stk::graph::temporary_vertex_graph_adaptor<Graph>& g )
	{
		return g.num_vertices();
	}

	template <typename Graph>
	inline std::pair<typename stk::graph::temporary_vertex_graph_adaptor<Graph>::vertex_iterator,
		typename stk::graph::temporary_vertex_graph_adaptor<Graph>::vertex_iterator>
	vertices( stk::graph::temporary_vertex_graph_adaptor<Graph>& g )
	{
		return g.vertices();
	}

	template <typename Graph>
	inline std::pair<typename stk::graph::temporary_vertex_graph_adaptor<Graph>::out_edge_iterator,
		typename stk::graph::temporary_vertex_graph_adaptor<Graph>::out_edge_iterator>
	out_edges( typename stk::graph::temporary_vertex_graph_adaptor<Graph>::vertex_descriptor v,
		const stk::graph::temporary_vertex_graph_adaptor<Graph>&                             g )
	{
		return g.out_edge_range( v );
	}

	template <typename Graph>
	inline std::size_t out_degree( typename stk::graph::temporary_vertex_graph_adaptor<Graph>::vertex_descriptor v,
		const stk::graph::temporary_vertex_graph_adaptor<Graph>&                                                 g )
	{
		auto r = out_edges( v, g );
		return std::distance( r.first, r.second );
	}

	template <typename Graph>
	inline std::size_t degree( typename stk::graph::temporary_vertex_graph_adaptor<Graph>::vertex_descriptor v,
		const stk::graph::temporary_vertex_graph_adaptor<Graph>&                                             g )
	{
		return out_degree( v, g );
	}

	template <typename Graph>
	inline typename stk::graph::temporary_vertex_graph_adaptor<Graph>::vertex_descriptor
	source( const typename stk::graph::temporary_vertex_graph_adaptor<Graph>::edge_descriptor& e,
		const stk::graph::temporary_vertex_graph_adaptor<Graph>& /*g*/ )
	{
		return e.source;
	}

	template <typename Graph>
	inline typename stk::graph::temporary_vertex_graph_adaptor<Graph>::vertex_descriptor
	target( const typename stk::graph::temporary_vertex_graph_adaptor<Graph>::edge_descriptor& e,
		const stk::graph::temporary_vertex_graph_adaptor<Graph>& /*g*/ )
	{
		return e.target;
	}

	//----------------------------------------------------------------------
	//! global_edge_iterator: Iterates over all the edges (joining each vertex’s out-edges)
	//----------------------------------------------------------------------
	template <typename Graph>
	class global_edge_iterator : public boost::iterator_facade<global_edge_iterator<Graph>, typename stk::graph::temporary_vertex_graph_adaptor<Graph>::edge_descriptor, boost::forward_traversal_tag, typename stk::graph::temporary_vertex_graph_adaptor<Graph>::edge_descriptor>
	{
	public:
		typedef stk::graph::temporary_vertex_graph_adaptor<Graph> adaptor_type;
		typedef typename adaptor_type::vertex_iterator            vertex_iterator;
		typedef typename adaptor_type::edge_descriptor            edge_descriptor;
		typedef typename adaptor_type::vertex_descriptor          vertex_descriptor;
		typedef typename adaptor_type::out_edge_iterator          out_edge_iterator;

		global_edge_iterator()
			: m_adaptor( nullptr )
		{}

		global_edge_iterator( const adaptor_type* adaptor,
			vertex_iterator                       v_current,
			vertex_iterator                       v_end )
			: m_adaptor( adaptor )
			, m_v_current( v_current )
			, m_v_end( v_end )
		{
			advance_to_valid();
		}

	private:
		friend class boost::iterator_core_access;
		void advance_to_valid()
		{
			while( m_v_current != m_v_end )
			{
				auto range = m_adaptor->out_edge_range( *m_v_current );
				m_current_edge = range.first;
				m_current_edge_end = range.second;
				if( m_current_edge != m_current_edge_end )
					return;
				++m_v_current;
			}
		}
		void increment()
		{
			++m_current_edge;
			if( m_current_edge == m_current_edge_end )
			{
				GEOMETRIX_ASSERT( m_v_current != m_v_end );
				++m_v_current;
				advance_to_valid();
			}
		}
		edge_descriptor dereference() const
		{
			return *m_current_edge;
		}
		bool equal( global_edge_iterator const& other ) const
		{
			return ( m_v_current == other.m_v_current ) && ( m_v_current == m_v_end || m_current_edge == other.m_current_edge );
		}

		const adaptor_type* m_adaptor;
		vertex_iterator     m_v_current, m_v_end;
		out_edge_iterator   m_current_edge, m_current_edge_end;
	};

	template <typename Graph>
	inline std::pair<global_edge_iterator<Graph>, global_edge_iterator<Graph>>
	edges( const stk::graph::temporary_vertex_graph_adaptor<Graph>& g )
	{
		auto                        v_range = g.vertices();
		global_edge_iterator<Graph> begin( &g, v_range.first, v_range.second );
		global_edge_iterator<Graph> end( &g, v_range.second, v_range.second );
		return std::make_pair( begin, end );
	}

	//----------------------------------------------------------------------
	//! Overloads for boost::add_edge()
	//!----------------------------------------------------------------------

	// Canonical overload when both vertices are provided as unified descriptors.
	template <typename Graph>
	inline std::pair<typename stk::graph::temporary_vertex_graph_adaptor<Graph>::edge_descriptor, bool>
	add_edge( typename stk::graph::temporary_vertex_graph_adaptor<Graph>::vertex_descriptor   u,
		typename stk::graph::temporary_vertex_graph_adaptor<Graph>::vertex_descriptor         v,
		const typename stk::graph::temporary_vertex_graph_adaptor<Graph>::edge_property_type& p,
		stk::graph::temporary_vertex_graph_adaptor<Graph>&                                    g )
	{
		return g.add_edge( u, v, p );
	}

	// The following overloads convert from original to unified vertex descriptors.
	template <typename Graph>
	inline auto add_edge( typename boost::graph_traits<Graph>::vertex_descriptor              u,
		typename stk::graph::temporary_vertex_graph_adaptor<Graph>::vertex_descriptor         v,
		const typename stk::graph::temporary_vertex_graph_adaptor<Graph>::edge_property_type& p,
		stk::graph::temporary_vertex_graph_adaptor<Graph>&                                    g )
		-> typename std::enable_if<!stk::graph::temporary_vertex_graph_adaptor<Graph>::using_integral,
			std::pair<typename stk::graph::temporary_vertex_graph_adaptor<Graph>::edge_descriptor, bool>>::type
	{
		auto unified_u = g.make_original_descriptor( u );
		return g.add_edge( unified_u, v, p );
	}

	template <typename Graph>
	inline auto add_edge( typename stk::graph::temporary_vertex_graph_adaptor<Graph>::vertex_descriptor u,
		typename boost::graph_traits<Graph>::vertex_descriptor                                          v,
		const typename stk::graph::temporary_vertex_graph_adaptor<Graph>::edge_property_type&           p,
		stk::graph::temporary_vertex_graph_adaptor<Graph>&                                              g )
		-> typename std::enable_if<!stk::graph::temporary_vertex_graph_adaptor<Graph>::using_integral,
			std::pair<typename stk::graph::temporary_vertex_graph_adaptor<Graph>::edge_descriptor, bool>>::type
	{
		auto unified_v = g.make_original_descriptor( v );
		return g.add_edge( u, unified_v, p );
	}

	template <typename Graph>
	inline auto add_edge( typename boost::graph_traits<Graph>::vertex_descriptor              u,
		typename boost::graph_traits<Graph>::vertex_descriptor                                v,
		const typename stk::graph::temporary_vertex_graph_adaptor<Graph>::edge_property_type& p,
		stk::graph::temporary_vertex_graph_adaptor<Graph>&                                    g )
		-> typename std::enable_if<!stk::graph::temporary_vertex_graph_adaptor<Graph>::using_integral,
			std::pair<typename stk::graph::temporary_vertex_graph_adaptor<Graph>::edge_descriptor, bool>>::type
	{
		auto unified_u = g.make_original_descriptor( u );
		auto unified_v = g.make_original_descriptor( v );
		return g.add_edge( unified_u, unified_v, p );
	}

	// Overloads for edge(u, v, g) -- returns the edge if one exists.
	template <typename Graph>
	inline std::pair<typename stk::graph::temporary_vertex_graph_adaptor<Graph>::edge_descriptor, bool>
	edge( typename stk::graph::temporary_vertex_graph_adaptor<Graph>::vertex_descriptor u,
		typename stk::graph::temporary_vertex_graph_adaptor<Graph>::vertex_descriptor   v,
		const stk::graph::temporary_vertex_graph_adaptor<Graph>&                        g )
	{
		using adaptor_type = stk::graph::temporary_vertex_graph_adaptor<Graph>;
		using edge_descriptor = typename adaptor_type::edge_descriptor;
		auto range = boost::out_edges( u, g );
		for( auto it = range.first; it != range.second; ++it )
		{
			if( boost::target( *it, g ) == v )
				return std::make_pair( *it, true );
		}
		return std::make_pair( edge_descriptor(), false );
	}

	// Overloads for edge() when one or both vertex descriptors are original.
	template <typename Graph>
	inline auto edge( typename boost::graph_traits<Graph>::vertex_descriptor          u,
		typename stk::graph::temporary_vertex_graph_adaptor<Graph>::vertex_descriptor v,
		const stk::graph::temporary_vertex_graph_adaptor<Graph>&                      g )
		-> typename std::enable_if<!stk::graph::temporary_vertex_graph_adaptor<Graph>::using_integral,
			std::pair<typename stk::graph::temporary_vertex_graph_adaptor<Graph>::edge_descriptor, bool>>::type
	{
		auto unified_u = g.make_original_descriptor( u );
		return edge( unified_u, v, g );
	}

	template <typename Graph>
	inline auto edge( typename stk::graph::temporary_vertex_graph_adaptor<Graph>::vertex_descriptor u,
		typename boost::graph_traits<Graph>::vertex_descriptor                                      v,
		const stk::graph::temporary_vertex_graph_adaptor<Graph>&                                    g )
		-> typename std::enable_if<!stk::graph::temporary_vertex_graph_adaptor<Graph>::using_integral,
			std::pair<typename stk::graph::temporary_vertex_graph_adaptor<Graph>::edge_descriptor, bool>>::type
	{
		auto unified_v = g.make_original_descriptor( v );
		return edge( u, unified_v, g );
	}

	template <typename Graph>
	inline auto edge( typename boost::graph_traits<Graph>::vertex_descriptor u,
		typename boost::graph_traits<Graph>::vertex_descriptor               v,
		const stk::graph::temporary_vertex_graph_adaptor<Graph>&             g )
		-> typename std::enable_if<!stk::graph::temporary_vertex_graph_adaptor<Graph>::using_integral,
			std::pair<typename stk::graph::temporary_vertex_graph_adaptor<Graph>::edge_descriptor, bool>>::type
	{
		auto unified_u = g.make_original_descriptor( u );
		auto unified_v = g.make_original_descriptor( v );
		return edge( unified_u, unified_v, g );
	}

} // namespace boost

// --------------------------------------------------------------------------
//! temporary_vertex_graph_vertex_property_map
//! (Used when boost::get() is called with a pointer-to-member for a vertex property.)
// --------------------------------------------------------------------------
template <typename Graph, typename PropPtr>
struct temporary_vertex_graph_vertex_property_map
{
	typedef stk::graph::temporary_vertex_graph_adaptor<Graph>                                    Adaptor;
	typedef typename Adaptor::vertex_descriptor                                                  key_type;
	typedef decltype( ( *(Adaptor*)nullptr )[*( (key_type*)nullptr )].*std::declval<PropPtr>() ) value_type;
	typedef value_type                                                                           reference;
	typedef boost::lvalue_property_map_tag                                                       category;

	temporary_vertex_graph_vertex_property_map( Adaptor* g, PropPtr p )
		: m_g( g )
		, m_prop( p )
	{}

	reference operator[]( const key_type& k ) const
	{
		return ( ( *m_g )[k] ).*m_prop;
	}

	Adaptor* m_g;
	PropPtr  m_prop;
};

// --------------------------------------------------------------------------
//! temporary_vertex_graph_edge_property_map
//! (Used when boost::get() is called with a pointer-to-member for an edge property.)
// --------------------------------------------------------------------------
template <typename Graph, typename PropPtr>
struct temporary_vertex_graph_edge_property_map
{
	typedef stk::graph::temporary_vertex_graph_adaptor<Graph>                                    Adaptor;
	typedef typename Adaptor::edge_descriptor                                                    key_type;
	typedef decltype( ( *(Adaptor*)nullptr )[*( (key_type*)nullptr )].*std::declval<PropPtr>() ) value_type;
	typedef value_type                                                                           reference;
	typedef boost::lvalue_property_map_tag                                                       category;

	temporary_vertex_graph_edge_property_map( Adaptor* g, PropPtr p )
		: m_g( g )
		, m_prop( p )
	{}

	reference operator[]( const key_type& k ) const
	{
		return ( ( *m_g )[k] ).*m_prop;
	}

	Adaptor* m_g;
	PropPtr  m_prop;
};

// --------------------------------------------------------------------------
//! Free function overloads for boost::get() with temporary_vertex_graph_adaptor.
//! These allow using pointer-to-member get() syntax such as:
//!    auto pm = boost::get(&VertexProperties::label, adaptor);
//! and then using: pm[v] to yield the desired property.
// --------------------------------------------------------------------------
namespace boost {

	template <typename Graph, typename PropPtr>
	inline temporary_vertex_graph_vertex_property_map<Graph, PropPtr>
	get( PropPtr p, stk::graph::temporary_vertex_graph_adaptor<Graph>& g )
	{
		return temporary_vertex_graph_vertex_property_map<Graph, PropPtr>( &g, p );
	}

	template <typename Graph, typename PropPtr>
	inline temporary_vertex_graph_vertex_property_map<Graph, PropPtr>
	get( PropPtr p, const stk::graph::temporary_vertex_graph_adaptor<Graph>& g )
	{
		return temporary_vertex_graph_vertex_property_map<Graph, PropPtr>( const_cast<stk::graph::temporary_vertex_graph_adaptor<Graph>*>( &g ), p );
	}

	template <typename Graph, typename PropPtr>
	inline temporary_vertex_graph_edge_property_map<Graph, PropPtr>
	get( PropPtr p, stk::graph::temporary_vertex_graph_adaptor<Graph>& g )
	{
		return temporary_vertex_graph_edge_property_map<Graph, PropPtr>( &g, p );
	}

	template <typename Graph, typename PropPtr>
	inline temporary_vertex_graph_edge_property_map<Graph, PropPtr>
	get( PropPtr p, const stk::graph::temporary_vertex_graph_adaptor<Graph>& g )
	{
		return temporary_vertex_graph_edge_property_map<Graph, PropPtr>( const_cast<stk::graph::temporary_vertex_graph_adaptor<Graph>*>( &g ), p );
	}

	//----------------------------------------------------------------------
	//! Overloads for boost::get() for property maps on temporary_vertex_graph_adaptor
	//!----------------------------------------------------------------------

	// For vertex property maps, when a pointer-to-member is provided,
	// if the key is a unified vertex descriptor...
	template <typename Graph, typename PropPtr, typename std::enable_if<stk::graph::temporary_vertex_graph_adaptor<Graph>::using_integral == false, int>::type = 0>
	inline auto get( PropPtr                                                                 p,
		stk::graph::temporary_vertex_graph_adaptor<Graph>&                                   g,
		const typename stk::graph::temporary_vertex_graph_adaptor<Graph>::vertex_descriptor& key )
		-> decltype( ( g[key] ).*p )
	{
		return ( g[key] ).*p;
	}
	template <typename Graph, typename PropPtr, typename std::enable_if<stk::graph::temporary_vertex_graph_adaptor<Graph>::using_integral == false, int>::type = 0>
	inline auto get( PropPtr                                                                 p,
		const stk::graph::temporary_vertex_graph_adaptor<Graph>&                             g,
		const typename stk::graph::temporary_vertex_graph_adaptor<Graph>::vertex_descriptor& key )
		-> decltype( ( g[key] ).*p )
	{
		return ( g[key] ).*p;
	}

	// For the original vertex descriptor keys, use boost::get on the underlying graph:
	template <typename Graph, typename PropPtr>
	inline auto get( PropPtr                                          p,
		stk::graph::temporary_vertex_graph_adaptor<Graph>&            g,
		const typename boost::graph_traits<Graph>::vertex_descriptor& key )
		-> decltype( g[key].*p )
	{
		return g[key].*p;
	}
	template <typename Graph, typename PropPtr>
	inline auto get( PropPtr                                          p,
		const stk::graph::temporary_vertex_graph_adaptor<Graph>&      g,
		const typename boost::graph_traits<Graph>::vertex_descriptor& key )
		-> decltype( g[key].*p )
	{
		return g[key].*p;
	}

	// For edge property maps on unified edge descriptors:
	template <typename Graph, typename PropPtr>
	inline auto get( PropPtr                                                               p,
		stk::graph::temporary_vertex_graph_adaptor<Graph>&                                 g,
		const typename stk::graph::temporary_vertex_graph_adaptor<Graph>::edge_descriptor& key )
		-> decltype( ( g[key] ).*p )
	{
		return ( g[key] ).*p;
	}
	template <typename Graph, typename PropPtr>
	inline auto get( PropPtr                                                               p,
		const stk::graph::temporary_vertex_graph_adaptor<Graph>&                           g,
		const typename stk::graph::temporary_vertex_graph_adaptor<Graph>::edge_descriptor& key )
		-> decltype( g[key].*p )
	{
		return g[key].*p;
	}

	// For original edge descriptors, use boost::get on the underlying graph:
	template <typename Graph, typename PropPtr>
	inline auto get( PropPtr                                        p,
		stk::graph::temporary_vertex_graph_adaptor<Graph>&          g,
		const typename boost::graph_traits<Graph>::edge_descriptor& key )
		-> decltype( g[key].*p )
	{
		return g[key].*p;
	}
	template <typename Graph, typename PropPtr>
	inline auto get( PropPtr                                        p,
		const stk::graph::temporary_vertex_graph_adaptor<Graph>&    g,
		const typename boost::graph_traits<Graph>::edge_descriptor& key )
		-> decltype( g[key].*p )
	{
		return g[key].*p;
	}
} // namespace boost

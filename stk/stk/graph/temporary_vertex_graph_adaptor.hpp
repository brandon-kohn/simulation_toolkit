//
//! Copyright � 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_GRAPH_TEMPORARY_VERTEX_GRAPH_ADAPTOR_HPP
#define STK_GRAPH_TEMPORARY_VERTEX_GRAPH_ADAPTOR_HPP

#include <stk/geometry/geometry_kernel.hpp>

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/properties.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/iterator/iterator_adaptor.hpp>
#include <boost/container/flat_map.hpp>
#include <stk/utility/boost_unique_ptr.hpp>
#include <stk/utility/associative_insert.hpp>

namespace stk {

    template <typename T, typename EnableIf=void> struct is_directed_adjacency_list : std::false_type {};
    template <typename OutEdgeListS, typename VertexListS, typename VertexProperty, typename EdgeProperty, typename GraphProperty, typename EdgeListS>
    struct is_directed_adjacency_list<boost::adjacency_list<OutEdgeListS, VertexListS, boost::directedS, VertexProperty, EdgeProperty, GraphProperty, EdgeListS>> : std::true_type {};

    template <typename T, typename EnableIf = void> struct has_vector_vertex_list : std::false_type {};
    template <typename OutEdgeListS, typename Directed, typename VertexProperty, typename EdgeProperty, typename GraphProperty, typename EdgeListS>
    struct has_vector_vertex_list<boost::adjacency_list<OutEdgeListS, boost::vecS, Directed, VertexProperty, EdgeProperty, GraphProperty, EdgeListS>> : std::true_type {};

    namespace detail {
        template <class NewVertexContainer, class Derived, class Config, class Base>
        inline typename Config::vertex_descriptor create_descriptor(const typename Config::stored_vertex& v, const NewVertexContainer&, const boost::adj_list_impl<Derived, Config, Base>&)
        {
            return &v;
        }

        template <class NewVertexContainer, class Derived, class Config, class Base>
        inline typename Config::vertex_descriptor create_descriptor(const typename Config::stored_vertex& /*v*/, const NewVertexContainer& ag, const boost::vec_adj_list_impl<Derived, Config, Base>& g_)
        {
            const Derived& g = static_cast<const Derived&>(g_);
            return g.m_vertices.size() + ag.size();
        }

        template <class Derived, class Config, class Base>
        inline typename Config::stored_vertex& get_stored_vertex(const typename Config::vertex_descriptor v, const boost::adj_list_impl<Derived, Config, Base>&)
        {
            return *(static_cast<typename Config::stored_vertex*>(v));
        }

        template <class Graph, class Config, class Base>
        inline typename Config::stored_vertex& get_stored_vertex(const typename Config::vertex_descriptor v, const boost::vec_adj_list_impl<Graph, Config, Base>& g_)
        {
            Graph& g = const_cast<Graph&>(static_cast<const Graph&>(g_));
            return g.m_vertices[v];
        }

        template <typename EdgeDescriptor, typename VertexDescriptor, typename EdgePropertyType, typename OutEdgeList>
        inline std::pair<EdgeDescriptor, bool> create_edge(VertexDescriptor u, VertexDescriptor v, const EdgePropertyType& p, OutEdgeList& oel)
        {
            //! Note this only works for directed adjacency lists. Unidirection and bidirection are not supported.
            using StoredEdge = typename OutEdgeList::value_type;
            typename OutEdgeList::iterator i;
            bool inserted;
            boost::tie(i, inserted) = boost::graph_detail::push(oel, StoredEdge(v, p));
            return std::make_pair(EdgeDescriptor(u, v, &(*i).get_property()), inserted);
        }

    }

    struct temporary_vertex_adapted_graph_tag {};

    template <typename T>
    struct temporary_vertex_graph_adaptor
    {
        static_assert(is_directed_adjacency_list<T>::value, "Can only be used with boost::adjacency_list which uses boost::directedS.");

        using graph_t = T;
        using vertex_descriptor = typename boost::graph_traits<T>::vertex_descriptor;
        using edge_descriptor = typename boost::graph_traits<T>::edge_descriptor;
        using stored_vertex = typename graph_t::stored_vertex;
        using out_edge_list_type = decltype(stored_vertex::m_out_edges);
        using stored_edge = typename out_edge_list_type::value_type;
        using vertex_property_type = typename graph_t::vertex_property_type;
        using edge_property_type = typename graph_t::edge_property_type;
        using EdgeContainer = typename graph_t::EdgeContainer;
        using out_edge_iterator = typename graph_t::out_edge_iterator;
        using adjacency_iterator = typename boost::adjacency_iterator_generator<temporary_vertex_graph_adaptor<T>, vertex_descriptor, out_edge_iterator>::type;
        using base_vertex_iterator = typename boost::graph_traits<graph_t>::vertex_iterator;
        using new_vertex_iterator = typename std::vector<vertex_descriptor>::iterator;
        class vertex_iterator : public boost::iterator_facade<vertex_iterator, vertex_descriptor, boost::bidirectional_traversal_tag, vertex_descriptor>
        {
        public:
            vertex_iterator(base_vertex_iterator it = base_vertex_iterator(), base_vertex_iterator end = base_vertex_iterator(), new_vertex_iterator nit = new_vertex_iterator(), temporary_vertex_graph_adaptor<T>* pGraph = nullptr)
            : mIT(it)
            , mEnd(end)
            , mNewIT(nit)
            , mGraph(pGraph)
            {}

        private:

            vertex_descriptor dereference() const
            {
                GEOMETRIX_ASSERT(mNewIT != mGraph->mOrderedNewVertices.end());
                if (mIT != mEnd)
                    return *mIT;
                return *mNewIT;
            }

            bool equal(const vertex_iterator& other) const
            {
                GEOMETRIX_ASSERT(mGraph == other.mGraph);
                return mIT == other.mIT && mNewIT == other.mNewIT;
            }

            void increment()
            {
                if (mIT != mEnd)
                    ++mIT;
                else
                    ++mNewIT;
            }

            void decrement()
            {
                if (mNewIT == mGraph->mOrderedNewVertices.begin())
                    --mIT;
                else
                    --mNewIT;
            }

            base_vertex_iterator mIT;
            base_vertex_iterator mEnd;
            new_vertex_iterator mNewIT;
            temporary_vertex_graph_adaptor<T>* mGraph;

            friend class boost::iterator_core_access;
        };
        using edge_iterator = boost::detail::adj_list_edge_iterator<vertex_iterator, out_edge_iterator, temporary_vertex_graph_adaptor<T>>;
        using graph_tag = temporary_vertex_adapted_graph_tag;
        using graph_property_type = typename graph_t::graph_property_type;
        using vertex_bundled = typename graph_t::vertex_bundled;
        using edge_bundled = typename graph_t::edge_bundled;
        using graph_bundled = typename graph_t::graph_bundled;
        using directed_category = typename graph_t::directed_category;
        using new_vertex_map_t = boost::container::flat_map<vertex_descriptor, std::unique_ptr<stored_vertex>>;

        static vertex_descriptor null_vertex() { return graph_t::null_vertex(); }

        temporary_vertex_graph_adaptor(const graph_t& graph)
            : mGraph(graph)
        {}

        temporary_vertex_graph_adaptor(const graph_t& graph, const vertex_property_type& newV, const std::vector<std::pair<vertex_descriptor, edge_property_type>>& newAdjacencies)
        : mGraph(graph)
        {
            auto v = add_vertex(newV);
            stored_vertex* vs = mAdaptedVertices.begin()->second.get();
            for (const auto& item : newAdjacencies)
                detail::create_edge<edge_descriptor>(v, item.first, item.second, vs->m_out_edges);
        }

        temporary_vertex_graph_adaptor(const temporary_vertex_graph_adaptor&) = delete;
        temporary_vertex_graph_adaptor& operator=(const temporary_vertex_graph_adaptor&) = delete;
        temporary_vertex_graph_adaptor(temporary_vertex_graph_adaptor&&) = delete;
        temporary_vertex_graph_adaptor& operator=(temporary_vertex_graph_adaptor&&) = delete;

        vertex_descriptor add_vertex()
        {
            auto pStorage = boost::make_unique<stored_vertex>();
            auto v = detail::create_descriptor(*pStorage, mOrderedNewVertices, mGraph);
            mOrderedNewVertices.push_back(v);
            mAdaptedVertices[v] = boost::move(pStorage);
            return v;
        }

        vertex_descriptor add_vertex(const vertex_property_type& p)
        {
            auto pStorage = boost::make_unique<stored_vertex>(p);
            auto v = detail::create_descriptor(*pStorage, mOrderedNewVertices, mGraph);
            mOrderedNewVertices.push_back(v);
            mAdaptedVertices[v] = boost::move(pStorage);
            return v;
        }

        std::pair<edge_descriptor, bool> add_edge(vertex_descriptor u, vertex_descriptor v, const edge_property_type& p)
        {
            auto it = mAdaptedVertices.lower_bound(u);
            stored_vertex* pSV = nullptr;
            if (!map_lower_bound_contains(u, mAdaptedVertices, it))
            {
                //! assume it is a static vertex from the original graph.
                auto pStorage = boost::make_unique<stored_vertex>(get_vertex_property(u));
                pSV = pStorage.get();
                auto& oldStorage = detail::get_stored_vertex(u, mGraph);
                for (const auto& item : oldStorage.m_out_edges)
                    pStorage->m_out_edges.emplace_back(item.get_target(), item.get_property());
                mAdaptedVertices.insert(it, std::make_pair(u, std::move(pStorage)));
            }
            else
                pSV = it->second.get();

            return detail::create_edge<edge_descriptor>(u, v, p, pSV->m_out_edges);
        }

        out_edge_list_type& out_edge_list(vertex_descriptor v)
        {
            auto it = mAdaptedVertices.find(v);
            if (it == mAdaptedVertices.end())
                return const_cast<graph_t&>(mGraph).out_edge_list(v);

            return it->second->m_out_edges;
        }

        const out_edge_list_type& out_edge_list(vertex_descriptor v) const
        {
            auto it = mAdaptedVertices.find(v);
            if (it == mAdaptedVertices.end())
                return mGraph.out_edge_list(v);

            return it->second->m_out_edges;
        }

        vertex_iterator vertices_begin() const
        {
            auto its = boost::vertices(mGraph);
            auto pThis = const_cast<temporary_vertex_graph_adaptor<T>*>(this);
            return vertex_iterator(its.first, its.second, pThis->mOrderedNewVertices.begin(), pThis);
        }

        vertex_iterator vertices_end() const
        {
            auto its = boost::vertices(mGraph);
            auto pThis = const_cast<temporary_vertex_graph_adaptor<T>*>(this);
            return vertex_iterator(its.second, its.second, pThis->mOrderedNewVertices.end(), pThis);
        }

        std::size_t num_vertices() const { return boost::num_vertices(mGraph) + mOrderedNewVertices.size(); }

        edge_iterator edges_begin() const
        {
            return edge_iterator(vertices_begin(), vertices_begin(), vertices_end(), *this);
        }

        edge_iterator edges_end() const
        {
            return edge_iterator(vertices_begin(), vertices_end(), vertices_end(), *this);
        }

        template <typename Reference, typename Tag>
        Reference get_vertex_property_value(vertex_descriptor v, Tag t)
        {
            auto it = mAdaptedVertices.find(v);
            if (it == mAdaptedVertices.end())
                return boost::get_property_value(detail::get_stored_vertex(v, mGraph).m_property, t);

            return boost::get_property_value(it->second->m_property, t);
        }

        const vertex_property_type& get_vertex_property(vertex_descriptor v) const
        {
            auto it = mAdaptedVertices.find(v);
            if (it == mAdaptedVertices.end())
                return detail::get_stored_vertex(v, mGraph).m_property;

            return it->second->m_property;
        }

        const vertex_property_type& operator[](vertex_descriptor v) const
        {
            return get_vertex_property(v);
        }

        const edge_property_type& get_edge_property(edge_descriptor e) const
        {
            return *(edge_property_type const*)e.get_property();
        }

        const edge_property_type& operator[](edge_descriptor e) const
        {
            return get_edge_property(e);
        }

        bool is_adapted_vertex(vertex_descriptor v) const { return mAdaptedVertices.find(v) != mAdaptedVertices.end(); }
        std::vector<vertex_descriptor> const& get_new_vertices() const { return mOrderedNewVertices; }

    private:

        friend class vertex_iterator;

        const graph_t&                  mGraph;
        new_vertex_map_t                mAdaptedVertices;
        std::vector<vertex_descriptor>  mOrderedNewVertices;
    };

}//! namespace stk;

namespace boost {

    template <typename T>
    struct graph_traits<stk::temporary_vertex_graph_adaptor<T>>
    {
    private:
        using adapted_t = stk::temporary_vertex_graph_adaptor<T>;

    public:

        using vertex_descriptor = typename graph_traits<T>::vertex_descriptor;
        using edge_descriptor = typename graph_traits<T>::edge_descriptor;

        using adjacency_iterator = typename adapted_t::adjacency_iterator;
        using out_edge_iterator = typename adapted_t::out_edge_iterator;
        using vertex_iterator = typename adapted_t::vertex_iterator;
        using edge_iterator = typename adapted_t::edge_iterator;

        using directed_category = directed_tag;
        using edge_parallel_category = allow_parallel_edge_tag; // not sure here
        using traversal_category = typename graph_traits<T>::traversal_category;
        using vertices_size_type = std::size_t;
        using edges_size_type = std::size_t;
        using degree_size_type = std::size_t;
    };

    // O(1)
    template <typename T>
    inline typename stk::temporary_vertex_graph_adaptor<T>::vertex_descriptor add_vertex(stk::temporary_vertex_graph_adaptor<T>& g)
    {
        return g.add_vertex();
    }
    // O(1)
    template <typename T>
    inline typename stk::temporary_vertex_graph_adaptor<T>::vertex_descriptor add_vertex(const typename stk::temporary_vertex_graph_adaptor<T>::vertex_property_type& p, stk::temporary_vertex_graph_adaptor<T>& g)
    {
        return g.add_vertex(p);
    }
    // O(V)
    template <typename T>
    inline typename stk::temporary_vertex_graph_adaptor<T>::vertex_descriptor vertex(typename stk::temporary_vertex_graph_adaptor<T>::vertices_size_type n, const stk::temporary_vertex_graph_adaptor<T>& g)
    {
        auto i = vertices(g).first;
        while (n--) ++i; // std::advance(i, n); (not VC++ portable)
        return *i;
    }

    template <typename T>
    inline std::pair<typename stk::temporary_vertex_graph_adaptor<T>::edge_descriptor, bool> add_edge(typename stk::temporary_vertex_graph_adaptor<T>::vertex_descriptor u, typename stk::temporary_vertex_graph_adaptor<T>::vertex_descriptor v, const typename stk::temporary_vertex_graph_adaptor<T>::edge_property_type& p, stk::temporary_vertex_graph_adaptor<T>& g)
    {
        return g.add_edge(u, v, p);
    }

    template <typename T>
    inline std::pair<typename stk::temporary_vertex_graph_adaptor<T>::edge_descriptor, bool> add_edge(typename stk::temporary_vertex_graph_adaptor<T>::vertex_descriptor u, typename stk::temporary_vertex_graph_adaptor<T>::vertex_descriptor v, stk::temporary_vertex_graph_adaptor<T>& g)
    {
        typename stk::temporary_vertex_graph_adaptor<T>::edge_property_type p;
        return g.add_edge(u, v, p);
    }

    template <typename T>
    inline std::pair<typename stk::temporary_vertex_graph_adaptor<T>::out_edge_iterator, typename stk::temporary_vertex_graph_adaptor<T>::out_edge_iterator> out_edges(typename stk::temporary_vertex_graph_adaptor<T>::vertex_descriptor u, const stk::temporary_vertex_graph_adaptor<T>& g_)
    {
        typedef typename stk::temporary_vertex_graph_adaptor<T>::out_edge_iterator out_edge_iterator;
        auto& g = const_cast<stk::temporary_vertex_graph_adaptor<T>&>(g_);
        return std::make_pair(out_edge_iterator(g.out_edge_list(u).begin(), u), out_edge_iterator(g.out_edge_list(u).end(), u));
    }

    template <typename T>
    inline std::pair<typename stk::temporary_vertex_graph_adaptor<T>::adjacency_iterator, typename stk::temporary_vertex_graph_adaptor<T>::adjacency_iterator> adjacent_vertices(typename stk::temporary_vertex_graph_adaptor<T>::vertex_descriptor u, const stk::temporary_vertex_graph_adaptor<T>& g)
    {
        using adjacency_iterator = typename stk::temporary_vertex_graph_adaptor<T>::adjacency_iterator;
        auto edges = out_edges(u, g);
        return std::make_pair(adjacency_iterator(edges.first, &g), adjacency_iterator(edges.second, &g));
    }

    template <typename Directed, typename Vertex, typename T>
    inline Vertex source(const detail::edge_base<Directed, Vertex>& e, const stk::temporary_vertex_graph_adaptor<T>&)
    {
        return e.m_source;
    }

    template <typename Directed, typename Vertex, typename T>
    inline Vertex target(const detail::edge_base<Directed, Vertex>& e, const stk::temporary_vertex_graph_adaptor<T>&)
    {
        return e.m_target;
    }

    template <typename T>
    inline std::pair<typename stk::temporary_vertex_graph_adaptor<T>::vertex_iterator, typename stk::temporary_vertex_graph_adaptor<T>::vertex_iterator> vertices(const stk::temporary_vertex_graph_adaptor<T>& g_)
    {
        auto& g = const_cast<stk::temporary_vertex_graph_adaptor<T>&>(g_);
        return std::make_pair(g.vertices_begin(), g.vertices_end());
    }

    template <typename T>
    inline std::size_t num_vertices(const stk::temporary_vertex_graph_adaptor<T>& g)
    {
        return g.num_vertices();
    }

    template <typename T>
    inline std::size_t out_degree(typename stk::temporary_vertex_graph_adaptor<T>::vertex_descriptor u, const stk::temporary_vertex_graph_adaptor<T>& g)
    {
        return g.out_edge_list(u).size();
    }

    template <typename T>
    inline std::size_t degree(typename stk::temporary_vertex_graph_adaptor<T>::vertex_descriptor u, const stk::temporary_vertex_graph_adaptor<T>& g)
    {
        return g.out_edge_list(u).size();
    }

    template <typename T>
    inline std::pair<typename stk::temporary_vertex_graph_adaptor<T>::edge_descriptor, bool> edge(typename stk::temporary_vertex_graph_adaptor<T>::vertex_descriptor u, typename stk::temporary_vertex_graph_adaptor<T>::vertex_descriptor v, const stk::temporary_vertex_graph_adaptor<T>& g)
    {
        using StoredEdge = typename stk::temporary_vertex_graph_adaptor<T>::stored_edge;
        const auto& el = g.out_edge_list(u);
        auto it = graph_detail::find(el, StoredEdge(v));
        return std::make_pair(typename stk::temporary_vertex_graph_adaptor<T>::edge_descriptor(u, v, (it == el.end() ? 0 : &(*it).get_property())),(it != el.end()));
    }

    template <typename T>
    inline std::pair<typename stk::temporary_vertex_graph_adaptor<T>::edge_iterator, typename stk::temporary_vertex_graph_adaptor<T>::edge_iterator> edges(const stk::temporary_vertex_graph_adaptor<T>& g_)
    {
        auto& g = const_cast<stk::temporary_vertex_graph_adaptor<T>&>(g_);
        return std::make_pair(g.edges_begin(), g.edges_end());
    }

    //=========================================================================
    // Vertex Property Maps

    template <class Graph, class ValueType, class Reference, class Tag>
    struct adapted_graph_vertex_property_map : public boost::put_get_helper<Reference, adapted_graph_vertex_property_map<Graph, ValueType, Reference, Tag>>
    {
        typedef ValueType value_type;
        typedef Reference reference;
        typedef typename Graph::vertex_descriptor key_type;
        typedef boost::lvalue_property_map_tag category;
        inline adapted_graph_vertex_property_map(const Graph* pGraph = nullptr, Tag tag = Tag())
        : mGraphPtr(pGraph)
        , mTag(tag)
        {}

        inline reference operator[](key_type v) const
        {
            auto pGraph = const_cast<Graph*>(mGraphPtr);
            return pGraph->template get_vertex_property_value<reference>(v, mTag);
        }

        inline Reference operator()(key_type v) const
        {
            return this->operator[](v);
        }

        const Graph*    mGraphPtr;
        Tag             mTag;
    };

    template <class Graph, class Property, class PropRef>
    struct adapted_graph_vertex_all_properties_map : public boost::put_get_helper<PropRef, adapted_graph_vertex_all_properties_map<Graph, Property, PropRef>>
    {
        typedef Property value_type;
        typedef PropRef reference;
        typedef typename Graph::vertex_descriptor key_type;
        typedef boost::lvalue_property_map_tag category;
        inline adapted_graph_vertex_all_properties_map(const Graph* pGraph= nullptr, vertex_all_t = vertex_all_t())
        : mGraphPtr(pGraph)
        {}

        inline PropRef operator[](key_type v) const
        {
            auto pGraph = const_cast<Graph*>(mGraphPtr);
            return pGraph->get_vertex_property(v);
        }

        inline PropRef operator()(key_type v) const
        {
            return this->operator[](v);
        }

        const Graph*    mGraphPtr;
    };

    template <class Property, class Vertex>
    struct adapted_graph_vertex_id_map : public boost::put_get_helper<Vertex, adapted_graph_vertex_id_map<Property, Vertex>>
    {
        typedef Vertex value_type;
        typedef Vertex key_type;
        typedef Vertex reference;
        typedef boost::readable_property_map_tag category;

        adapted_graph_vertex_id_map() { }
        template <class Graph>
        adapted_graph_vertex_id_map(const Graph&, vertex_index_t) { }
        value_type operator[](key_type v) const { return v; }
        value_type operator()(key_type v) const { return v; }
    };

    struct adapted_graph_vertex_id_pa
    {
        template <class Tag, class Graph, class Property>
        struct bind_
        {
            using vertex_t = typename Graph::vertex_descriptor;
            typedef adapted_graph_vertex_id_map<Property, vertex_t> type;
            typedef adapted_graph_vertex_id_map<Property, vertex_t> const_type;
        };
    };

    struct adapted_graph_any_vertex_pa
    {
        template <class Tag, class Graph, class Property>
        struct bind_
        {
            typedef typename property_value<Property, Tag>::type value_type;
            typedef value_type& reference;
            typedef const value_type& const_reference;

            typedef adapted_graph_vertex_property_map<Graph, value_type, reference, Tag> type;
            typedef adapted_graph_vertex_property_map<Graph, value_type, const_reference, Tag> const_type;
        };
    };

    struct adapted_graph_all_vertex_pa
    {
        template <class Tag, class Graph, class Property>
        struct bind_
        {
            typedef typename Graph::vertex_descriptor Vertex;
            typedef adapted_graph_vertex_all_properties_map<Graph, Property, Property&> type;
            typedef adapted_graph_vertex_all_properties_map<Graph, Property, const Property&> const_type;
        };
    };

    namespace detail {
        template <class Tag, class Graph, class Property, class EnableIf=void>
        struct adapted_graph_choose_vertex_pa : adapted_graph_any_vertex_pa::template bind_<Tag, Graph, Property>
        {};

        template <class Graph, class Property>
        struct adapted_graph_choose_vertex_pa<vertex_all_t, Graph, Property> : adapted_graph_all_vertex_pa::template bind_<vertex_all_t, Graph, Property>
        {};

        template <class Graph, class Property>
        struct adapted_graph_choose_vertex_pa<vertex_index_t, Graph, Property, typename std::enable_if<stk::has_vector_vertex_list<typename Graph::graph_t>::value>::type> : adapted_graph_vertex_id_pa::template bind_<vertex_index_t, Graph, Property>
        {};
    } // namespace detail

    //=========================================================================
    // Edge Property Map

    template <class Directed, class Value, class Ref, class Vertex, class Property, class Tag>
    struct adapted_graph_edge_property_map : public put_get_helper<Ref, adapted_graph_edge_property_map<Directed, Value, Ref, Vertex, Property, Tag>>
    {
        Tag tag;
        explicit adapted_graph_edge_property_map(Tag tag = Tag()) : tag(tag) {}

        typedef Value value_type;
        typedef Ref reference;
        typedef detail::edge_desc_impl<Directed, Vertex> key_type;
        typedef boost::lvalue_property_map_tag category;
        inline Ref operator[](key_type e) const
        {
            Property& p = *(Property*)e.get_property();
            return get_property_value(p, tag);
        }
        inline Ref operator()(key_type e) const
        {
            return this->operator[](e);
        }
    };

    template <class Directed, class Property, class PropRef, class PropPtr, class Vertex>
    struct adapted_graph_edge_all_properties_map : public put_get_helper<PropRef, adapted_graph_edge_all_properties_map<Directed, Property, PropRef, PropPtr, Vertex>>
    {
        explicit adapted_graph_edge_all_properties_map(edge_all_t = edge_all_t()) {}
        typedef Property value_type;
        typedef PropRef reference;
        typedef detail::edge_desc_impl<Directed, Vertex> key_type;
        typedef boost::lvalue_property_map_tag category;
        inline PropRef operator[](key_type e) const
        {
            return *(PropPtr)e.get_property();
        }
        inline PropRef operator()(key_type e) const
        {
            return this->operator[](e);
        }
    };

    // Edge Property Maps

    namespace detail {
        struct adapted_graph_any_edge_pmap
        {
            template <class Graph, class Property, class Tag>
            struct bind_
            {
                typedef typename property_value<Property, Tag>::type value_type;
                typedef value_type& reference;
                typedef const value_type& const_reference;

                typedef adapted_graph_edge_property_map<typename Graph::directed_category, value_type, reference, typename Graph::vertex_descriptor, Property, Tag> type;
                typedef adapted_graph_edge_property_map<typename Graph::directed_category, value_type, const_reference, typename Graph::vertex_descriptor, const Property, Tag> const_type;
            };
        };
        struct adapted_graph_all_edge_pmap
        {
            template <class Graph, class Property, class Tag>
            struct bind_
            {
                typedef adapted_graph_edge_all_properties_map<typename Graph::directed_category, Property, Property&, Property*, typename Graph::vertex_descriptor> type;
                typedef adapted_graph_edge_all_properties_map<typename Graph::directed_category, Property, const Property&, const Property*, typename Graph::vertex_descriptor> const_type;
            };
        };

        template <class Tag>
        struct adapted_graph_choose_edge_pmap_helper
        {
            typedef adapted_graph_any_edge_pmap type;
        };
        template <>
        struct adapted_graph_choose_edge_pmap_helper<edge_all_t>
        {
            typedef adapted_graph_all_edge_pmap type;
        };
        template <class Tag, class Graph, class Property>
        struct adapted_graph_choose_edge_pmap : adapted_graph_choose_edge_pmap_helper<Tag>::type::template bind_<Graph, Property, Tag>
        {};
        struct adapted_graph_edge_property_selector
        {
            template <class Graph, class Property, class Tag>
            struct bind_ : adapted_graph_choose_edge_pmap<Tag, Graph, Property> {};
        };
    } // namespace detail

    template <>
    struct edge_property_selector<stk::temporary_vertex_adapted_graph_tag>
    {
        typedef detail::adapted_graph_edge_property_selector type;
    };

    // Vertex Property Maps

    struct adapted_graph_vertex_property_selector
    {
        template <class Graph, class Property, class Tag>
        struct bind_ : detail::adapted_graph_choose_vertex_pa<Tag, Graph, Property>
        {};
    };

    template <>
    struct vertex_property_selector<stk::temporary_vertex_adapted_graph_tag>
    {
        typedef adapted_graph_vertex_property_selector type;
    };

    namespace detail {
        template <typename T, typename Property>
        inline typename boost::property_map<stk::temporary_vertex_graph_adaptor<T>, Property>::type get_dispatch(stk::temporary_vertex_graph_adaptor<T>&, Property p, boost::edge_property_tag)
        {
            typedef typename boost::property_map<stk::temporary_vertex_graph_adaptor<T>, Property>::type PA;
            return PA(p);
        }

        template <typename T, typename Property>
        inline typename boost::property_map<stk::temporary_vertex_graph_adaptor<T>,Property>::const_type get_dispatch(const stk::temporary_vertex_graph_adaptor<T>&, Property p, boost::edge_property_tag)
        {
            typedef stk::temporary_vertex_graph_adaptor<T> Graph;
            typedef typename boost::property_map<Graph, Property>::const_type PA;
            return PA(p);
        }

        template <typename T, typename Property>
        inline typename boost::property_map<stk::temporary_vertex_graph_adaptor<T>, Property>::type get_dispatch(stk::temporary_vertex_graph_adaptor<T>& g, Property p, boost::vertex_property_tag)
        {
            typedef typename boost::property_map<stk::temporary_vertex_graph_adaptor<T>, Property>::type PA;
            return PA(&g, p);
        }

        template <typename T, typename Property>
        inline typename boost::property_map<stk::temporary_vertex_graph_adaptor<T>, Property>::const_type get_dispatch(const stk::temporary_vertex_graph_adaptor<T>& g, Property p, boost::vertex_property_tag)
        {
            typedef typename boost::property_map<stk::temporary_vertex_graph_adaptor<T>, Property>::const_type PA;
            return PA(&g, p);
        }

    } // namespace detail

    // Implementation of the PropertyGraph interface
    template <typename T, typename Property>
    inline typename boost::property_map<stk::temporary_vertex_graph_adaptor<T>, Property>::type get(Property p, stk::temporary_vertex_graph_adaptor<T>& g)
    {
        typedef typename detail::property_kind_from_graph<stk::temporary_vertex_graph_adaptor<T>, Property>::type Kind;
        return detail::get_dispatch(g, p, Kind());
    }

    template <typename T, typename Property>
    inline typename boost::property_map<stk::temporary_vertex_graph_adaptor<T>, Property>::const_type get(Property p, const stk::temporary_vertex_graph_adaptor<T>& g)
    {
        typedef typename detail::property_kind_from_graph<stk::temporary_vertex_graph_adaptor<T>, Property>::type Kind;
        return detail::get_dispatch(g, p, Kind());
    }

    template <typename T, typename Property, typename Key>
    inline typename boost::property_traits<typename boost::property_map<stk::temporary_vertex_graph_adaptor<T>,Property>::type>::reference get(Property p, stk::temporary_vertex_graph_adaptor<T>& g, const Key& key)
    {
        return get(get(p, g), key);
    }

    template <typename T, typename Property, typename Key>
    inline typename boost::property_traits< typename boost::property_map<stk::temporary_vertex_graph_adaptor<T>, Property>::const_type >::reference get(Property p, const stk::temporary_vertex_graph_adaptor<T>& g, const Key& key)
    {
        return get(get(p, g), key);
    }

}//! namespace boost

#endif //! STK_GRAPH_TEMPORARY_VERTEX_GRAPH_ADAPTOR_HPP


//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_CONCURRENTSKIPLIST_HPP
#define STK_CONCURRENTSKIPLIST_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include <geometrix/utility/assert.hpp>
#include <geometrix/utility/utilities.hpp>
#include <stk/utility/boost_unique_ptr.hpp>

#include <boost/iterator.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/mpl/int.hpp>
#include <boost/operators.hpp>
#include <boost/array.hpp>
#include <boost/optional.hpp>

#ifdef BOOST_NO_CXX11_THREAD_LOCAL
#include <boost/thread/tss.hpp>
#endif

#include <random>
#include <list>
#include <mutex>
#include <type_traits>
#include <memory>

//! This factor is the exponent that 2 is raised to and should cover the number of threads expected to use the skip list.
//! e.g. default is 4 = 2^4 == 16 threads.
#define STK_SKIPLIST_THREAD_IDS_FACTOR 4

namespace stk { namespace detail {

    //! Associative map traits to make the lazy list behave like a map<key, value>
    template < typename Key,             //! key type
        typename Value,                  //! mapped type
        typename Pred,                   //! comparator predicate type
        typename Alloc,                  //! actual allocator type (should be value allocator)
        unsigned int MaxLevel,           //! Max level of the skip list node array.
        bool allowMultipleKeys = false > //! true if multiple equivalent keys are permitted
    struct associative_map_traits
    {
        typedef Key                                                key_type;
        typedef std::pair<const Key, Value>                        value_type;
        typedef Pred                                               key_compare;
        typedef typename Alloc::template rebind<value_type>::other allocator_type;
        typedef boost::mpl::int_<MaxLevel>                         max_level;
        typedef boost::mpl::bool_<allowMultipleKeys>               allow_multiple_keys;

        associative_map_traits(Pred _Parg, Alloc _Al)
            : m_compare(_Parg)
        {}

        class value_compare
        {
            friend struct associative_map_traits<Key, Value, Pred, Alloc, allowMultipleKeys>;

        public:

            using result_type = bool;

            bool operator()(const value_type& _Left, const value_type& _Right) const
            {
                return (m_compare(_Left.first, _Right.first));
            }

            value_compare(key_compare _Pred)
                : m_compare(_Pred)
            {}

        protected:
            key_compare m_compare;  // the comparator predicate for keys
        };

        static const Key& resolve_key( const value_type& v )
        {
            return (v.first);
        }

        Pred m_compare; // the comparator predicate for keys
    };

    //! Associative map traits to make the lazy list behave like a map<key, value>
    template < typename Key,             //! key type
        typename Pred,                   //! comparator predicate type
        typename Alloc,                  //! actual allocator type (should be value allocator)
        unsigned int MaxLevel,           //! Max level of the skip list node array.
        bool AllowMultipleKeys = false > //! true if multiple equivalent keys are permitted
    struct associative_set_traits
    {
        typedef Key                                                key_type;
        typedef Key                                                value_type;
        typedef Pred                                               key_compare;
        typedef typename Alloc::template rebind<value_type>::other allocator_type;

        typedef boost::mpl::bool_<AllowMultipleKeys>  allow_multiple_keys;
        typedef boost::mpl::int_<MaxLevel>            max_level;

        associative_set_traits(Pred p, Alloc /*al*/)
            : m_compare(p)
        {}

        typedef key_compare value_compare;
        static const Key& resolve_key(const value_type& v)
        {
            return (v);
        }

        Pred m_compare; // the comparator predicate for keys
    };

    template <typename T, typename Alloc>
    class raii_allocator
    {
    public:

        raii_allocator( const Alloc& a )
            : al( a )
            , constructed( false )
            , memory(0)
        {
            //! this can throw bad_alloc or whatever if no memory.
            memory = al.allocate(1);
        }

        raii_allocator(const raii_allocator&) = delete;
        raii_allocator& operator =(const raii_allocator&) = delete;

        ~raii_allocator()
        {
            //! If memory is allocated but not constructed, the c'tor threw. Deallocate.
            if( 0 != memory && !constructed )
                al.deallocate(memory, 1);
        }

        //! Define the macro that expands into an instance of the function.
        template <typename ...Ts>
        T* create(Ts&& ... a)
        {
            new (memory) T(std::forward<Ts>(a)...);
            constructed = true;
            return memory;
        }

    private:

        bool  constructed;
        T*    memory;
        Alloc al;

    };

    template <typename T, typename Allocator, typename...Ts>
    inline T* construct_from_allocator(const Allocator& al, Ts&&... a)
    {
        return raii_allocator<T, Allocator>( al ).create(std::forward<Ts>(a)...);
    }

    // base list node type.
    template<typename AssociativeTraits>
    class skip_list_node : public AssociativeTraits
    {
    protected:
        struct          node;
        friend struct   node;
        typedef node*   node_ptr;

        typedef AssociativeTraits                          traits;
        typedef typename AssociativeTraits::allocator_type allocator_type;
        typedef typename AssociativeTraits::key_compare    key_compare;
        typedef typename AssociativeTraits::value_type     value_type;
        typedef typename AssociativeTraits::key_type       key_type;

        struct node : boost::totally_ordered< node, boost::totally_ordered< node, key_type > >
        {
            enum node_position_class
            {
                e_first,
                e_middle,
                e_last,
            };

            typedef std::vector< node_ptr > node_levels;

            node( node_position_class r )
                : bound( static_cast<char>(r) )
                , value_()
                , nexts( traits::max_level::value + 1 )
                , marked( false )
                , fullyLinked( false )
                , topLevel( traits::max_level::value )
            {}

            node( const value_type& v, unsigned int height )
                : bound( static_cast<char>( e_middle ) )
                , value_( v )
                , nexts( height + 1 )
                , marked( false )
                , fullyLinked( false )
                , topLevel( height )
            {}

            bool operator < ( const node& rhs ) const
            {
                return geometrix::lexicographical_compare( bound, key(), rhs.bound, rhs.key() );
            }

            bool operator < ( const key_type& rhs ) const
            {
                return geometrix::lexicographical_compare( bound, key(), static_cast<char>( e_middle ), rhs );
            }

            bool operator == ( const key_type& v ) const
            {
                return ! geometrix::lexicographical_compare( static_cast<char>( e_middle ), v, bound, key() ) &&
                       ! geometrix::lexicographical_compare( bound, key(), static_cast<char>( e_middle ), v );
            }

            bool operator == ( const node& n ) const
            {
                //return !( n.item_key() < item_key() ) && !( item_key() < n.item_key() );
                return ! geometrix::lexicographical_compare( n.bound, n.key()_, bound, key() ) &&
                       ! geometrix::lexicographical_compare( bound, key(), n.bound, key() );
            }

            const key_type&  key() const { return traits::resolve_key( value_ ); }
            value_type&      item() { return value_; }
            node_levels&     next() { return nexts; }
            bool             is_nil() { return bound != e_middle; }
            bool             is_head() { return bound == e_first; }
            bool             is_tail() { return bound == e_last; }

            char                   bound;
            value_type             value_;
            node_levels            nexts;
            volatile char          marked;
            volatile char          fullyLinked;
            unsigned int           topLevel;
            std::recursive_mutex   mutex;
        };

        skip_list_node( key_compare p, allocator_type al )
            : AssociativeTraits( p, al )
            , m_nodeAllocator( al )
        {}

        typedef typename allocator_type::template rebind<node>::other node_allocator;   // allocator object for nodes
        node_allocator m_nodeAllocator; // allocator object for nodes
    };

    // TEMPLATE CLASS _Tree_ptr
    template<typename AssociativeTraits>
    class skip_list_ptr : public skip_list_node<AssociativeTraits>
    {
    public:

        typedef skip_list_node<AssociativeTraits>          base;
        typedef typename base::node                        node;
        typedef typename base::node_ptr                    node_ptr;
        typedef typename AssociativeTraits::allocator_type allocator_type;
        typedef typename AssociativeTraits::key_compare    key_compare;

        skip_list_ptr( key_compare p, allocator_type al )
            : skip_list_node<AssociativeTraits>( p, al )
            , m_nodePtrAllocator( al )
        {}

        typename allocator_type::template rebind<node_ptr>::other m_nodePtrAllocator;    // allocator object for pointers to nodes
    };

    // TEMPLATE CLASS _Tree_val
    template<typename AssociativeTraits>
    class skip_list_val : public skip_list_ptr<AssociativeTraits>
    {
    protected:

        typedef typename AssociativeTraits::allocator_type allocator_type;
        typedef typename AssociativeTraits::key_compare key_compare;

        skip_list_val( key_compare p, allocator_type al )
            : skip_list_ptr<AssociativeTraits>( p, al )
            , m_nodeValueAllocator( al )
        {}

        allocator_type m_nodeValueAllocator;    // allocator object for values stored in nodes
    };

    template <typename RandomNumberGenerator>
    class thread_random_index_generator
    {
    public:

        thread_random_index_generator(std::size_t maxIndex)
            : m_distribution(0, maxIndex)
        {}

        //! Generate an integer on the interval [0,maxIndex).
        std::size_t operator()() const { return m_distribution(get_generator()); }
        void seed(unsigned long seed) { get_generator().seed(seed); m_distribution.reset(); }

    private:

        static RandomNumberGenerator& get_generator()
        {
#ifndef BOOST_NO_CXX11_THREAD_LOCAL
            thread_local RandomNumberGenerator generator;
#else
            static boost::thread_specific_ptr<RandomNumberGenerator> generator;
            auto result = generator.get();
            if (!result)
            {
                result = new RandomNumberGenerator();
                generator.reset(result);
            }
            return *result;
#endif
        }

        std::uniform_int_distribution<std::size_t> m_distribution;

    };
}

template <typename AssociativeTraits, typename LevelSelectionPolicy = detail::thread_random_index_generator<std::mt19937>>
class lazy_concurrent_skip_list : public detail::skip_list_val<AssociativeTraits>
{
public:
    typedef AssociativeTraits                          traits_type;
    typedef typename AssociativeTraits::key_type       key_type;
    typedef typename AssociativeTraits::value_type     value_type;
    typedef typename AssociativeTraits::key_compare    key_compare;
    typedef typename AssociativeTraits::allocator_type allocator_type;
    typedef typename allocator_type::reference         reference;
    typedef typename allocator_type::const_reference   const_reference;
    typedef LevelSelectionPolicy                       level_selector;

    template <typename T>
    class node_iterator;

    template <typename T>
    friend class node_iterator;

    template <typename T>
    class node_iterator
        : public boost::iterator_facade
        <
            node_iterator<T>
            , T
            , boost::forward_traversal_tag
        >
    {
    public:

        friend class lazy_concurrent_skip_list< traits_type, level_selector >;

        node_iterator()
            : m_pNode()
        {}

        explicit node_iterator( node* pNode )
            : m_pNode( pNode )
        {}

    private:

        friend class boost::iterator_core_access;

        void increment()
        {
            if( m_pNode && !m_pNode->is_tail() )
            {
                m_pNode = m_pNode->next().front();
            }
        }

        bool equal( node_iterator<T> const& other) const
        {
            return m_pNode == other.m_pNode;
        }

        T& dereference() const
        {
            GEOMETRIX_ASSERT( m_pNode );
            return m_pNode->item();
        }

        node* m_pNode{ nullptr };
    };

    struct const_iterator;

    struct iterator : node_iterator< value_type >
    {
        iterator()
            : node_iterator< value_type >()
        {}

        explicit iterator( node* pNode )
            : node_iterator< value_type >( pNode )
        {}

        bool operator ==( node_iterator< value_type > const& other ) const
        {
            return m_pNode == other.m_pNode;
        }

        bool operator ==( node_iterator< const value_type > const& other ) const
        {
            return m_pNode == other.m_pNode;
        }

        bool operator !=( node_iterator< value_type > const& other ) const
        {
            return m_pNode != other.m_pNode;
        }

        bool operator !=( node_iterator< const value_type > const& other ) const
        {
            return m_pNode != other.m_pNode;
        }
    };

    struct const_iterator : node_iterator< const value_type >
    {
        const_iterator()
            : node_iterator< const value_type >()
        {}

        explicit const_iterator( node* pNode )
            : node_iterator< const value_type >( pNode )
        {}

        const_iterator( iterator const& other )
            : node_iterator< const value_type >( other.m_pNode )
        {}

        const_iterator operator =( iterator const& other )
        {
            m_pNode = other.m_pNode;
            return *this;
        }

        bool operator ==( iterator const& other ) const
        {
            return m_pNode == other.m_pNode;
        }

        bool operator ==( const_iterator const& other ) const
        {
            return m_pNode == other.m_pNode;
        }

        bool operator !=( iterator const& other ) const
        {
            return m_pNode != other.m_pNode;
        }

        bool operator !=( const_iterator const& other ) const
        {
            return m_pNode != other.m_pNode;
        }
    };

    lazy_concurrent_skip_list( const key_compare& pred = key_compare(), const allocator_type& al = allocator_type() )
        : detail::skip_list_val<AssociativeTraits>( pred, al )
        , m_selector( max_level() )
        , m_pHead()
        , m_pTail()
    {
        m_pHead = stk::detail::construct_from_allocator<node>( m_nodeAllocator, node::e_first ), make_dealloc(m_nodeAllocator);
        m_pTail = stk::detail::construct_from_allocator<node>( m_nodeAllocator, node::e_last ), make_dealloc(m_nodeAllocator);
        GEOMETRIX_ASSERT( m_pHead );
        if( m_pHead )
        {
            for( std::size_t i = 0; i < m_pHead->next().size(); ++i )
                m_pHead->next()[i] = m_pTail;
        }
    }

    lazy_concurrent_skip_list( const lazy_concurrent_skip_list& other )
        : detail::skip_list_val<AssociativeTraits>( other )
        , m_selector( max_level() )
        , m_pHead()
        , m_pTail()
    {


        m_pHead.reset( construct_from_allocator<node>( m_nodeAllocator, node::e_first ), make_dealloc(m_nodeAllocator) );
        m_pTail.reset( construct_from_allocator<node>( m_nodeAllocator, node::e_last ), make_dealloc(m_nodeAllocator) );
        GEOMETRIX_ASSERT( m_pHead );
        if( m_pHead )
        {
            for( std::size_t i = 0; i < m_pHead->next().size(); ++i )
                m_pHead->next()[i] = m_pTail;

            for( auto item : other )
            {
                insert( item );
            }
        }
    }

    lazy_concurrent_skip_list& operator = ( const lazy_concurrent_skip_list& other )
    {
        release();

        m_pHead.= construct_from_allocator<node>( m_nodeAllocator, node::e_first ), make_dealloc(m_nodeAllocator);
        m_pTail = construct_from_allocator<node>( m_nodeAllocator, node::e_last ), make_dealloc(m_nodeAllocator);
        GEOMETRIX_ASSERT( m_pHead );
        if( m_pHead )
        {
            for( std::size_t i = 0; i < m_pHead->next().size(); ++i )
                m_pHead->next()[i] = m_pTail;

            for( auto item : other )
            {
                insert( item );
            }
        }

        return *this;
    }

    ~lazy_concurrent_skip_list()
    {
        release();
    }

    iterator begin()
    {
        return ( iterator( left_most() ) );
    }

    const_iterator begin() const
    {
        return ( const_iterator ( left_most() ) );
    }

    iterator end()
    {
        return ( iterator( m_pTail ) );
    }

    const_iterator end() const
    {
        return ( const_iterator( m_pTail ) );
    }

    iterator find( const key_type& x )
    {
        boost::array<node*, max_level::value + 1> preds;
        boost::array<node*, max_level::value + 1> succs;

        int lFound = find( x, preds, succs );
        if( lFound != -1 )
        {
            node* pFound = succs[lFound];
            GEOMETRIX_ASSERT( pFound );
            if( !pFound )
                return end();
            if( pFound->fullyLinked && !pFound->marked )
                return iterator( pFound );
            else
                return end();
        }

        return end();
    }

    const_iterator find( const key_type& x ) const
    {
        boost::array<node*, max_level::value + 1> preds;
        boost::array<node*, max_level::value + 1> succs;

        int lFound = find( x, preds, succs );
        if( lFound != -1 )
        {
            auto pFound = succs[lFound];
            GEOMETRIX_ASSERT( pFound );
            if( !pFound )
                return end();
            if( pFound->fullyLinked && !pFound->marked )
                return const_iterator( pFound );
            else
                return end();
        }

        return end();
    }

    iterator lower_bound( const key_type& x )
    {
        boost::array<node*, max_level::value + 1> preds;
        boost::array<node*, max_level::value + 1> succs;

        int lFound = lower_bound( x, preds, succs );
        if( lFound != -1 )
        {
            auto pFound = succs[lFound];
            GEOMETRIX_ASSERT( pFound );
            if( !pFound )
                return end();
            if( pFound->fullyLinked && !pFound->marked )
                return iterator( pFound );
            else
                return end();
        }

        return end();
    }

    const_iterator lower_bound( const key_type& x ) const
    {
        boost::array<node*, max_level::value + 1> preds;
        boost::array<node*, max_level::value + 1> succs;

        int lFound = lower_bound( x, preds, succs );
        if( lFound != -1 )
        {
            auto pFound = succs[lFound];
            GEOMETRIX_ASSERT( pFound );
            if( !pFound )
                return end();
            if( pFound->fullyLinked && !pFound->marked )
                return const_iterator( pFound );
            else
                return end();
        }

        return end();
    }

    std::pair< iterator, bool > insert( const value_type& item )
    {
        return add( item );
    }

    iterator insert( const iterator&, const value_type& item )
    {
        return add( item ).first;
    }

    bool empty() const
    {
        return begin() == end();
    }

    key_compare key_comp() const { return m_compare; }

    bool contains( const key_type& x ) const
    {
        boost::array<node*, max_level::value + 1> preds;
        boost::array<node*, max_level::value + 1> succs;

        int lFound = find( x, preds, succs );
        if( lFound != -1 )
        {
            auto pFound = succs[lFound];
            GEOMETRIX_ASSERT( pFound );
            if( !pFound )
                return false;
            return (pFound->fullyLinked && !pFound->marked );
        }

        return false;
    }

protected:

    using lock = std::lock_guard<std::recursive_mutex>;
    using lock_ptr = std::unique_ptr< lock >;
    using lock_list = std::list< lock_ptr >;

    template <typename Alloc>
    struct deallocator
    {
        deallocator( Alloc& alloc )
            : alloc(alloc)
        {}

        template <typename T>
        void operator()( T* pT )
        {
            alloc.destroy(pT);
            alloc.deallocate(pT, 1);
        }

        Alloc& alloc;
    };

    template <typename Alloc>
    deallocator<Alloc> make_dealloc( Alloc& alloc ){ return deallocator<Alloc>(alloc); }

    //! Access the left most valid node.
    node* left_most() const
    {
        auto pNext = m_pHead->next()[0];
        GEOMETRIX_ASSERT( pNext );
        if( pNext )
            return pNext;
        else
            return m_pTail;
    }

    node_ptr create_node( node* pNext, const value_type& v )
    {
        // allocate a node and set links and value
        return stk::detail::construct_from_allocator<node>( m_nodeAllocator, resolve_key( v ), v, pNext );
    }

    node_ptr create_node( const value_type& v, unsigned int height )
    {
        // allocate a node and set links and value
        return stk::detail::construct_from_allocator<node>( m_nodeAllocator, v, height );
    }

    void destroy_node( node_ptr pNode )
    {
        m_nodeAllocator.destroy( pNode );
        m_nodeAllocator.deallocate( pNode, 1 );
    }

    void release()
    {
        clear();

        GEOMETRIX_ASSERT( m_pHead );
        //m_pHead.reset();
        if( m_pHead )
            destroy_node( m_pHead );
        m_pHead = nullptr;

        GEOMETRIX_ASSERT( m_pTail );
        //m_pTail.reset();
        if( m_pTail )
            destroy_node( m_pTail );
        m_pTail = nullptr;
    }

    template <typename Preds, typename Succs>
    int find( const key_type& key, Preds& preds, Succs& succs ) const
    {
        int lFound = -1;
        node* pPred = m_pHead;
        for( int level = max_level::value; level >= 0; --level )
        {
            node* pCurr = pPred->next()[level];
            GEOMETRIX_ASSERT( pCurr );
            while( pCurr && *pCurr < key ) //key > curr.key
            {
                pPred = pCurr;
                pCurr = pPred->next()[level];
            }

            if( pCurr && lFound == -1 && key == *pCurr )
                lFound = level;

            GEOMETRIX_ASSERT( static_cast<int>( preds.size() ) >= level );
            GEOMETRIX_ASSERT( static_cast<int>( succs.size() ) >= level );
            if( static_cast<int>( preds.size() ) >= level )
                preds[level] = pPred;
            if( static_cast<int>( succs.size() ) >= level )
                succs[level] = pCurr;
        }

        return lFound;
    }

    template <typename Preds, typename Succs>
    int lower_bound( const key_type& key, Preds& preds, Succs& succs ) const
    {
        int lFound = -1;
        node* pPred = m_pHead;
        for( int level = max_level(); level >= 0; --level )
        {
            node* pCurr = pPred->next()[level];
            GEOMETRIX_ASSERT( pCurr );
            while( pCurr && *pCurr < key ) //key > curr.key
            {
                pPred = pCurr;
                pCurr = pPred->next()[level];
            }

            if( pCurr && key <= *pCurr )
                lFound = level;

            GEOMETRIX_ASSERT( static_cast<int>( preds.size() ) >= level );
            GEOMETRIX_ASSERT( static_cast<int>( succs.size() ) >= level );
            if( static_cast<int>( preds.size() ) >= level )
                preds[level] = pPred;
            if( static_cast<int>( succs.size() ) >= level )
                succs[level] = pCurr;
        }

        return lFound;
    }

    std::pair< iterator, bool > add( const value_type& x )
    {
        int topLevel = m_selector();
        boost::array<node*, max_level::value + 1> preds;
        boost::array<node*, max_level::value + 1> succs;

        //! To unlock locks on exit...
        lock_list locks;

        while( true )
        {
            int lFound = find( resolve_key(x), preds, succs );
            if( lFound != -1 )
            {
                node* pFound = succs[lFound];
                GEOMETRIX_ASSERT( pFound );
                if( !pFound )
                    return std::make_pair( end(), false );

                if( !pFound->marked )
                {
                    while( !pFound->fullyLinked ){}
                    return std::make_pair( iterator( pFound ), false );
                }

                continue;
            }

            int highestLocked = -1;
            node* pPred;
            node* pSucc;
            bool valid = true;
            for( int level = 0; valid && level <= topLevel; ++level )
            {
                pPred = preds[level];
                pSucc = succs[level];

                //! add to the exit locks.
                locks.emplace_back( new lock(pPred->mutex ) );
                highestLocked = level;
                valid = !pPred->marked && !pSucc->marked && pPred->next()[level] == pSucc;
            }

            if( !valid )
            {
                locks.clear();
                continue;
            }

            node* pNewNode = create_node( x, topLevel );
            for( int level = 0; level <= topLevel; ++level )
                pNewNode->next()[level] = succs[level];
            for( int level = 0; level <= topLevel; ++level )
                preds[level]->next()[level] = pNewNode;
            pNewNode->fullyLinked = true;
            return std::make_pair( iterator( pNewNode ), true );
        }
    }

    iterator erase( const key_type& x )
    {
        node* pVictim;
        bool isMarked = false;
        int topLevel = -1;
        boost::array<node*, max_level::value + 1> preds;
        boost::array<node*, max_level::value + 1> succs;

        //! To unlock locks on exit...
        lock_list locks;
        std::unique_ptr<lock> victimLock;

        while( true )
        {
            int lFound = find( x, preds, succs );
            if( lFound != -1 )
            {
                pVictim = succs[lFound];
                GEOMETRIX_ASSERT( pVictim );
            }

            if(
                isMarked ||
                (
                    lFound != -1 &&
                    ( pVictim->fullyLinked && pVictim->topLevel == lFound && !pVictim->marked )
                )
              )
            {
                if( !isMarked )
                {
                    topLevel = pVictim->topLevel;

                    //! add to the exit locks.
                    victimLock = boost::make_unique<lock>( pVictim->mutex );
                    if( pVictim->marked )
                    {
                        //! Unlock it.
                        victimLock.reset();
                        return end();
                    }

                    pVictim->marked = true;
                    isMarked = true;
                }

                int highestLocked = -1;
                node* pPred;
                bool valid = true;
                for( int level = 0; valid && level <= topLevel; ++level )
                {
                    pPred = preds[level];

                    GEOMETRIX_ASSERT( pPred );
                    if( !pPred )
                        return end();

                    //! add to the exit locks.
                    locks.emplace_back(new lock{ pPred->mutex });

                    highestLocked = level;
                    valid = !pPred->marked && pPred->next()[level] == pVictim;
                }

                if( !valid )
                {
                    locks.clear();
                    continue;
                }

                for( int level = topLevel; level >= 0; --level )
                    preds[level]->next()[level] = pVictim->next()[level];

                //! Save return iterator.
                iterator nextIT( pVictim->next()[0] );

                victimLock.reset();

                destroy_node( pVictim );

                return nextIT;
            }
            else
                return end();
        }
    }

    iterator erase( const_iterator it )
    {
        if( it == end() )
            return end();

        const value_type& x = *it;
        return erase( resolve_key( x ) );
    }

    void clear()
    {
        for( iterator it(begin()); it != end(); )
        {
            const value_type& x = *it;

            //! increment before removing it.
            ++it;

            erase( resolve_key( x ) );
        }
    }

    node* m_pHead;
    node* m_pTail;

    level_selector m_selector;
};

//! Declare a set type.
template <typename Key, typename Compare = std::less< Key >, typename Alloc = std::allocator< Key > >
class concurrent_set : public lazy_concurrent_skip_list< detail::associative_set_traits< Key, Compare, Alloc, 32, false > >
{
    typedef lazy_concurrent_skip_list< detail::associative_set_traits< Key, Compare, Alloc, 32, false > > BaseType;

public:

    concurrent_set( const Compare& c = Compare() )
        : BaseType( c )
    {}
};

//! Declare a map type.
template <typename Key, typename Value, typename Compare = std::less< Key >, typename Alloc = std::allocator< Key > >
class concurrent_map : public lazy_concurrent_skip_list< detail::associative_map_traits< Key, Value, Compare, Alloc, 32, false > >
{
    typedef lazy_concurrent_skip_list< detail::associative_map_traits< Key, Value, Compare, Alloc, 32, false > > BaseType;

public:

    typedef Value                                              mapped_type;
    typedef Key                                                key_type;
    typedef typename boost::add_reference< mapped_type >::type reference;
    typedef typename boost::add_const< mapped_type >::type     const_reference;

    concurrent_map( const Compare& c = Compare() )
        : BaseType( c )
    {}

    reference operator [] ( const key_type& k )
    {
        iterator it = find( k );
        if( it == end() )
            it = insert( value_type( k, mapped_type() ) ).first;
        return (it->second);
    }
};

//! Declare a map type with a variable height parameter.
template <typename Key, typename Value, unsigned int MaxHeight, typename Compare = std::less< Key >, typename Alloc = std::allocator< Key > >
class skip_map : public lazy_concurrent_skip_list< detail::associative_map_traits< Key, Value, Compare, Alloc, MaxHeight, false > >
{
    typedef lazy_concurrent_skip_list< detail::associative_map_traits< Key, Value, Compare, Alloc, MaxHeight, false > > BaseType;

public:

    typedef Value                                              mapped_type;
    typedef Key                                                key_type;
    typedef typename boost::add_reference< mapped_type >::type reference;
    typedef typename boost::add_const< mapped_type >::type     const_reference;

    skip_map( const Compare& c = Compare() )
        : BaseType( c )
    {}

    reference operator [] ( const key_type& k )
    {
        iterator it = find( k );
        if( it == end() )
            it = insert( value_type( k, mapped_type() ) ).first;
        return (it->second);
    }
};

}//! namespace stk;

#endif//! STK_CONCURRENTSKIPLIST_HPP

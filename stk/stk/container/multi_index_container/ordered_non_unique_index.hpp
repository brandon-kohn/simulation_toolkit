#ifndef STK_CONTAINER_MULTI_INDEX_ORDERED_NON_UNIQUE_INDEX_HPP
#define STK_CONTAINER_MULTI_INDEX_ORDERED_NON_UNIQUE_INDEX_HPP
#pragma once

#include <stk/container/multi_index_container/multi_index_container.hpp>
#include <stk/container/multi_index_container/detail/indirect_compare.hpp>

#include <boost/container/flat_set.hpp>
#include <boost/iterator/iterator_adaptor.hpp>

namespace stk {
    
    template <typename Collection, typename Iterator, typename Value>
    class ordered_non_unique_index_iterator 
        : public boost::iterator_adaptor
          <
              ordered_non_unique_index_iterator< Collection, Iterator, Value >// Derived
            , Iterator                  // Base
            , Value                     // Value
            , boost::use_default        // Category of Traversal
            , Value&                    // Reference
          >
    {
        struct enabler {};
        typedef typename ordered_non_unique_index_iterator::iterator_adaptor_ super;
        friend class boost::iterator_core_access;
        typedef Iterator iterator;    

    public:

        ordered_non_unique_index_iterator()
            : super( iterator() )
            , m_pCollection(nullptr)
        {}
                
        explicit ordered_non_unique_index_iterator( Collection* c, Iterator it )
            : super(it)
            , m_pCollection(c)
        {}

        template <typename OtherCollection, typename OtherIterator, typename OtherValue>
        ordered_non_unique_index_iterator
            (
                ordered_non_unique_index_iterator<OtherCollection, OtherIterator, OtherValue> const& other
              , typename boost::enable_if_c
                <
                    boost::is_convertible<OtherValue*,Value*>::value && boost::is_convertible<OtherCollection*, Collection*>::value
                  , enabler
                >::type = enabler() 
            )
            : super(other.base())
        {}

        std::size_t get_index() const { return *this->base_reference(); }

    private:

        typename Value& dereference() const
        {
            std::size_t index = *this->base_reference();
            assert(m_pCollection);
            if( !m_pCollection )
                throw std::bad_exception();//null iterator dereference.
            return m_pCollection->at(index);
        }

        Collection* m_pCollection;
    };

    //! An ordered index which holds the integer index of the value in underlying collection in the index.
    template <typename Tag, typename Value, typename Compare = std::less<Value> >
    struct ordered_non_unique_index
    {
        typedef boost::mpl::vector<Tag> TagList;

        template <typename Base>
        struct index_class : Base
        {
        private:
            typedef Base super;            
        protected:
            typedef typename super::base_container_type base_container_type;
        private:
            typedef boost::container::flat_multiset<std::size_t, detail::indirect_compare_adaptor<base_container_type, Compare> > container_type;

        public:
            index_class( const Compare& c = Compare() )
                : m_index(detail::indirect_compare_adaptor<base_container_type, Compare>(get_base_container(), c))
                , m_cmp(c)
            {}

            typedef boost::add_reference<Value>             reference;
            typedef boost::add_const<reference>             const_reference;
            typedef Value                                   value_type;
            typedef ordered_non_unique_index_iterator<base_container_type, typename container_type::iterator, Value>             iterator;
            typedef ordered_non_unique_index_iterator<const base_container_type, typename container_type::const_iterator, const Value> const_iterator;

            std::size_t size() const 
            {
                return m_index.size();
            }

            bool empty() const
            {
                return m_index.empty();
            }

            std::pair<iterator, iterator> equal_range(const Value& v)
            {
                //! Need an indirect way to compare values for std::lower_bound
                detail::indirect_compare_value_adaptor<base_container_type, Value, Compare> cmp(get_base_container(), v, m_cmp);
                auto it1 = std::lower_bound(m_index.begin(), m_index.end(), (std::numeric_limits<std::size_t>::max)(), cmp);
                auto it2 = std::upper_bound(m_index.begin(), m_index.end(), (std::numeric_limits<std::size_t>::max)(), cmp);
                return std::make_pair(iterator(&get_base_container(), it1), iterator(&get_base_container(), it2));
            }

            iterator       begin() { return iterator(&get_base_container(), m_index.begin()); }
            const_iterator begin() const { return const_iterator(&get_base_container(), m_index.begin()); }
            iterator       end() { return iterator(&get_base_container(), m_index.end()); }
            const_iterator end() const { return const_iterator(&get_base_container(), m_index.end()); }

            void insert(const Value& v)
            {
                get_most_derived().insert_impl(v);
            }

            void clear()
            {
                get_most_derived().clear_impl();
            }

            iterator erase(iterator it)
            {
                std::size_t i = it.get_index();

                std::size_t next = ++it==end() ? -1 : it.get_index() < i ? it.get_index() : it.get_index()-1;   
                get_most_derived().erase_impl(i);

                if( next == -1 )
                    return end();
                else
                    return iterator(&get_base_container(), m_index.find(next));
            }

        protected:

            std::size_t insert_impl(const Value& v)
            {
                std::size_t index = get_base_container().size();
                std::size_t res = super::insert_impl(v);
                if( res == index )
                    m_index.insert(index);             
                return res;
            }

            void clear_impl()
            {
                m_index.clear();
                super::clear_impl();
            }

            void erase_impl(std::size_t i)
            {
                iterator it = m_index.find(i);
                if( it != m_index.end() )
                    m_index.erase(it);
                
                //! Find all elements in the tree >= i and decrement them. Ouch.
                it = m_index.begin();
                while( it != m_index.end() )
                {
                    if( *it >= i )
                        --*it;//! On some sad day, flat_set might decide to make the reference const... if that happens use a wrapper Index<int> type that has a mutable decrement.
                    ++it;                
                }

                super::erase_impl(i);
            }
                        
        private:
            container_type m_index;
            Compare        m_cmp;
        };    
    };
    
}//! namespace stk;

#endif // STK_CONTAINER_MULTI_INDEX_ORDERED_NON_UNIQUE_INDEX_HPP


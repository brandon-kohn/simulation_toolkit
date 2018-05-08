#ifndef STK_CONTAINER_MULTI_INDEX_CONTAINER_MULTI_INDEX_CONTAINER_HPP
#define STK_CONTAINER_MULTI_INDEX_CONTAINER_MULTI_INDEX_CONTAINER_HPP
#pragma once

#include <stk/container/multi_index_container/detail/has_tag.hpp>
#include <boost/mpl/vector.hpp>

namespace stk { 

    template <typename Value, typename IndexSeq>
    struct multi_index_container;

    template <typename Value, typename IndexSeq>
    struct multi_index_container_base
    {
    protected:
        typedef IndexSeq                                     index_sequence_type;
        typedef multi_index_container<Value, IndexSeq>       most_derived_type;
        
        typedef std::vector<Value>                           base_container_type;
        typedef typename base_container_type::iterator       iterator;
        typedef typename base_container_type::const_iterator const_iterator;  

    public:
        typedef boost::add_reference<Value>                  reference;
        typedef boost::add_const<reference>                  const_reference;
        typedef Value                                        value_type;
        
    protected:
        most_derived_type&         get_most_derived() { return static_cast<most_derived_type&>(*this); }
        base_container_type&       get_base_container() { return m_elements; }
        base_container_type const& get_base_container() const { return m_elements; }

        std::size_t    size() const { return m_elements.size(); }

        iterator       begin() { return m_elements.begin(); }
        const_iterator begin() const { return m_elements.begin(); }
        iterator       end() { return m_elements.end(); }
        const_iterator end() const { return m_elements.end(); }

        std::size_t insert_impl(const Value& v)
        {
            return create_node(v);
        }

        void clear_impl()
        {
            m_elements.clear();
        }

        void erase_impl(std::size_t i)
        {
            destroy_node(i);
        }

    private:

        std::size_t create_node(const Value& v)
        {            
            m_elements.push_back(v);
            return m_elements.size() - 1;
        }

        void destroy_node(std::size_t i)
        {
            m_elements.erase(m_elements.begin() + i);
        }
    
        base_container_type m_elements;
    };
    
    struct apply_index
    {
        template <typename IndexType, typename IndexGenerator>
        struct apply
            : boost::mpl::identity< typename IndexType::type::template index_class<typename IndexGenerator::type> >
        {};
    };

    template <int N, typename Value, typename IndexSeq>
    struct multi_index_container_index_generator
        : boost::mpl::eval_if_c
          <
              N == boost::mpl::size<IndexSeq>::value
            , boost::mpl::identity< multi_index_container_base<Value, IndexSeq> >
            , boost::mpl::apply2
              <
                  apply_index
                , boost::mpl::at_c<IndexSeq, N>
                , multi_index_container_index_generator
                  <
                      N+1
                    , Value
                    , IndexSeq
                  >
              >
          >
    {};

    template <typename Value, typename IndexSeq>
    struct multi_index_container
        : multi_index_container_index_generator<0, Value, IndexSeq>::type
    {
        typedef typename multi_index_container_index_generator<0, Value, IndexSeq>::type super;
                
        //! Access the type of the Nth index.
        template <int N>
        struct nth_index
        {
            typedef typename multi_index_container_index_generator<N, Value, IndexSeq>::type type;
            typedef typename boost::add_reference<type>::type  reference;
            typedef typename boost::add_const<reference>::type const_reference;
        };

        template <typename Tag>
        struct index
        {
        private:
            typedef typename boost::mpl::find_if
                <
                    IndexSeq
                  , Detail::HasTag<Tag>
                >::type iter;
            BOOST_STATIC_CONSTANT(bool,index_found=!(boost::is_same<iter,typename boost::mpl::end<IndexSeq>::type >::value));
            BOOST_STATIC_ASSERT(index_found);
        public:
            
            typedef typename boost::mpl::deref<iter>::type::template index_class
                <
                    typename multi_index_container_index_generator
                    <
                        boost::mpl::distance<typename boost::mpl::begin<IndexSeq>::type, iter>::value+1
                      , Value
                      , IndexSeq
                    >::type
                > type;

            typedef typename boost::add_reference<type>::type  reference;
            typedef typename boost::add_const<reference>::type const_reference;
        };

        template <int N>
        typename nth_index<N>::reference get_index()
        {
            return *this;
        }

        template <int N>
        typename nth_index<N>::const_reference get_index() const
        {
            return *this;
        }

        template <int N>
        typename nth_index<N>::reference get()
        {
            return *this;
        }

        template <int N>
        typename nth_index<N>::const_reference get() const
        {
            return *this;
        }

        template <typename Tag>
        typename Index<Tag>::reference get_index()
        {
            return *this;
        }

        template <typename Tag>
        typename Index<Tag>::const_reference get_index() const
        {
            return *this;
        }

        template <typename Tag>
        typename Index<Tag>::reference get()
        {
            return *this;
        }

        template <typename Tag>
        typename Index<Tag>::const_reference get() const
        {
            return *this;
        }

        std::size_t insert_impl(const Value& v)
        {
            return super::insert_impl(v);
        }

        void clear_impl()
        {
            super::clear_impl();
        }

        void erase_impl(std::size_t i)
        {
            if( get_base_container().size() > i )
                super::erase_impl(i);
        }
    };

}//! namespace stk;

#endif // STK_CONTAINER_MULTI_INDEX_CONTAINER_MULTI_INDEX_CONTAINER_HPP


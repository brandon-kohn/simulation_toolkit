#ifndef STK_CONTAINER_MULTI_INDEX_RANDOM_ACCESS_INDEX_HPP
#define STK_CONTAINER_MULTI_INDEX_RANDOM_ACCESS_INDEX_HPP
#pragma once

#include <stk/container/multi_index_container/multi_index_container.hpp>

namespace stk { 
    
    template <typename Tag, typename Value>
    struct random_access_index 
    {
        typedef boost::mpl::vector<Tag> TagList;

        template <typename Base>
        struct index_class : Base
        {
        private:
            typedef Base super;

        public:

            typedef typename boost::add_reference<Value>::type          reference;
            typedef typename boost::add_const<reference>::type          const_reference;
            typedef Value                                               value_type;
            typedef typename super::base_container_type::iterator       iterator;
            typedef typename super::base_container_type::const_iterator const_iterator;

            iterator       begin() { return get_base_container().begin(); }
            const_iterator begin() const { return get_base_container().begin(); }
            iterator       end() { return get_base_container().end(); }
            const_iterator end() const { return get_base_container().end(); }

            reference       operator[](std::size_t index) { return get_base_container()[index]; }
            const_reference operator[](std::size_t index) const { return get_base_container()[index]; }

            std::size_t size() const 
            {
                return get_base_container().size(); 
            }

            bool empty() const
            {
                return get_base_container().empty();
            }
            
            void push_back(const Value& v)
            {
                get_most_derived().insert_impl(v);
            }

            void clear()
            {
                get_most_derived().clear_impl();
            }

            iterator erase(iterator it)
            {
                std::size_t i = std::distance(begin(), it);
                get_most_derived().erase_impl(i);
                if( i < size() )
                    return begin() + i;
                else
                    return end();
            }

        protected:

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
                super::erase_impl(i);
            }
        };
    };

}//! namespace stk;

#endif // STK_CONTAINER_MULTI_INDEX_RANDOM_ACCESS_INDEX_HPP



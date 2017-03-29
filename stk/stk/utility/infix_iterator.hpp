//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_INFIX_ITERATOR_HPP
#define STK_INFIX_ITERATOR_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

// Lifted from Jerry Coffin's 's prefix_ostream_iterator 
#include <ostream> 
#include <iterator> 

namespace stk {

template <class T,
    class charT = char,
    class traits = std::char_traits<charT> >
    class infix_ostream_iterator :
    public std::iterator<std::output_iterator_tag, void, void, void, void>
{
    std::basic_ostream<charT, traits> *os;
    charT const* delimiter;
    bool first_elem;
public:
    typedef charT char_type;
    typedef traits traits_type;
    typedef std::basic_ostream<charT, traits> ostream_type;
    infix_ostream_iterator(ostream_type& s)
        : os(&s), delimiter(0), first_elem(true)
    {}
    infix_ostream_iterator(ostream_type& s, charT const *d)
        : os(&s), delimiter(d), first_elem(true)
    {}
    infix_ostream_iterator<T, charT, traits>& operator=(T const &item)
    {
        // Here's the only real change from ostream_iterator: 
        // Normally, the '*os << item;' would come before the 'if'. 
        if (!first_elem && delimiter != 0)
            *os << delimiter;
        *os << item;
        first_elem = false;
        return *this;
    }
    infix_ostream_iterator<T, charT, traits> &operator*() {
        return *this;
    }
    infix_ostream_iterator<T, charT, traits> &operator++() {
        return *this;
    }
    infix_ostream_iterator<T, charT, traits> &operator++(int) {
        return *this;
    }
};

}//! namespace stk;

#endif//STK_INFIX_ITERATOR_HPP

//
//! Copyright © 2013-2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
//! Original implementation from "The Art of Multiprocessor Programming",
//! @book{Herlihy:2008 : AMP : 1734069,
//! author = { Herlihy, Maurice and Shavit, Nir },
//! title = { The Art of Multiprocessor Programming },
//! year = { 2008 },
//! isbn = { 0123705916, 9780123705914 },
//! publisher = { Morgan Kaufmann Publishers Inc. },
//! address = { San Francisco, CA, USA },
//}
#pragma once

#include <geometrix/utility/assert.hpp>

#include <boost/iterator.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/mpl/int.hpp>
#include <stk/thread/tiny_atomic_spin_lock.hpp>
#include <stk/utility/scope_exit.hpp>
#include <stk/utility/boost_unique_ptr.hpp>

#include <array>
#include <random>
#include <list>
#include <mutex>
#include <type_traits>
#include <memory>

//! A concurrent skip list implementation details.
#define STK_SKIP_LIST_MAX_HEIGHT 64

namespace stk { namespace detail {

    //! Associative map traits to make the lazy list behave like a map<key, value>
    template < typename Key,             //! key type
        typename Value,                  //! mapped type
        typename Pred,                   //! comparator predicate type
        typename Alloc,                  //! actual allocator type (should be value allocator)
        unsigned int MaxHeight,           //! Max size of the skip list node array.
        bool allowMultipleKeys = false,  //! true if multiple equivalent keys are permitted
        typename Mutex = stk::thread::tiny_atomic_spin_lock<>>
    struct associative_map_traits
    {
        typedef Key                                                key_type;
        typedef std::pair<const Key, Value>                        value_type;
        typedef Pred                                               key_compare;
        using allocator_type = typename std::allocator_traits<Alloc>::template rebind_alloc<value_type> ;
        typedef boost::mpl::int_<MaxHeight>                        max_height;
        typedef boost::mpl::int_<MaxHeight-1>                      max_level;
        typedef boost::mpl::bool_<allowMultipleKeys>               allow_multiple_keys;
        typedef Mutex                                              mutex_type;
        using size_type = std::size_t;

        associative_map_traits(Alloc)
        {}

        class value_compare
        {
            friend struct associative_map_traits<Key, Value, Pred, Alloc, MaxHeight, allowMultipleKeys, Mutex>;

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

        BOOST_FORCEINLINE static const Key& resolve_key( const value_type& v )
        {
            return (v.first);
        }
    };

    //! Associative map traits to make the lazy list behave like a map<key, value>
    template < typename Key,             //! key type
        typename Pred,                   //! comparator predicate type
        typename Alloc,                  //! actual allocator type (should be value allocator)
        unsigned int MaxHeight,          //! Max size of the skip list node array.
        bool AllowMultipleKeys = false,  //! true if multiple equivalent keys are permitted
        typename Mutex = stk::thread::tiny_atomic_spin_lock<>>
    struct associative_set_traits
    {
        typedef Key                                                key_type;
        typedef Key                                                value_type;
        typedef Pred                                               key_compare;
        typedef typename Alloc::template rebind<value_type>::other allocator_type;
        typedef Mutex                                              mutex_type;
        typedef boost::mpl::bool_<AllowMultipleKeys>               allow_multiple_keys;
        typedef boost::mpl::int_<MaxHeight>                        max_height;
        typedef boost::mpl::int_<MaxHeight-1>                      max_level;
    using size_type = std::size_t;

        associative_set_traits(Alloc /*al*/)
        {}

        typedef key_compare value_compare;
        BOOST_FORCEINLINE static const Key& resolve_key(const value_type& v)
        {
            return v;
        }

    };

    class random_xor_shift_generator
    {
    public:

        using result_type = unsigned int;
        BOOST_CONSTEXPR static result_type min BOOST_PREVENT_MACRO_SUBSTITUTION() { return 0; }
        BOOST_CONSTEXPR static result_type max BOOST_PREVENT_MACRO_SUBSTITUTION() { return (std::numeric_limits<result_type>::max)(); }

        random_xor_shift_generator(unsigned int seed = 42)
        {
            m_state.store(seed, std::memory_order_relaxed);
        }

        random_xor_shift_generator(const random_xor_shift_generator&rhs)
            : m_state(rhs.m_state.load(std::memory_order_relaxed))
        {

        }

        random_xor_shift_generator& operator=(const random_xor_shift_generator&rhs)
        {
            m_state.store(rhs.m_state.load(std::memory_order_relaxed), std::memory_order_relaxed);
            return *this;
        }

        BOOST_FORCEINLINE unsigned int operator()()
        {
            auto x = m_state.load(std::memory_order_relaxed);
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            m_state.store(x, std::memory_order_relaxed);
            return x;
        }
    private:

        std::atomic<unsigned int> m_state;

    };

    template <std::uint8_t MaxHeight>
    struct size_table : std::array<std::uint64_t, MaxHeight>
    {
        size_table()
        {
            for (int i = 0; i < MaxHeight; ++i)
                (*this)[i] = (std::min)(std::uint64_t(1ULL << i), (std::numeric_limits<std::uint64_t>::max)());
        }
    };

    inline static size_table<STK_SKIP_LIST_MAX_HEIGHT> const& get_size_table()
    {
        static size_table<STK_SKIP_LIST_MAX_HEIGHT> instance;
        return instance;
    }
}//! namespace detail;

template <std::uint8_t MaxHeight>
struct skip_list_level_selector
{
    using max_level = std::integral_constant<std::uint8_t, MaxHeight - 1>;

    struct probability_table
    {
        probability_table(double p = 0.5)
        {
            probabilities[0] = 1.0;
            for (int i = 1; i < MaxHeight; ++i)
                probabilities[i] = std::pow(p, i);
        }

        std::array<double, MaxHeight>        probabilities;
    };

    static probability_table const& get_probs()
    {
        static probability_table instance;
        return instance;
    }

    static detail::random_xor_shift_generator create_generator()
    {
        std::random_device rd;
        return detail::random_xor_shift_generator(rd());
    }

    static double rnd()
    {
        static detail::random_xor_shift_generator eng = create_generator();
        static std::uniform_real_distribution<> dist;
        return dist(eng);
    }

    std::uint8_t operator()(std::uint8_t maxLevel = max_level::value) const
    {
        auto lvl = std::uint8_t{ 0 };
        auto const& probs = get_probs().probabilities;
        while (rnd() < probs[lvl + 1] && lvl < maxLevel)
            ++lvl;
        return lvl;
    }
};

template <std::uint8_t MaxHeight>
struct coin_flip_level_selector
{
    using max_level = std::integral_constant<std::uint8_t, MaxHeight - 1>;

    static detail::random_xor_shift_generator create_generator()
    {
        std::random_device rd;
        return detail::random_xor_shift_generator(rd());
    }

    static std::uint64_t rnd()
    {
        static detail::random_xor_shift_generator eng = create_generator();
        std::uniform_int_distribution<std::uint64_t> dist;
        return dist(eng);
    }

    std::uint8_t operator()(std::uint8_t maxLevel = max_level::value) const
    {
        auto x = rnd();
        if ((x & 1) != 0)
            return 0;

        auto lvl = std::uint8_t{ 1 };
        while ((((x >>= 1) & 1) != 0) && lvl < maxLevel)
            ++lvl;
        return lvl;
    }
};

}//! namespace stk;

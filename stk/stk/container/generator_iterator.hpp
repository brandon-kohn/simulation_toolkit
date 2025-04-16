// (C) Copyright Jens Maurer 2001.
// (C) Copyright Brandon Kohn 2021.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// Revision History:

// 15 Nov 2001   Jens Maurer
//      created.
// 30 Apr 2021   Brandon Kohn - modified to use lambdas.

//  See http://www.boost.org/libs/utility/iterator_adaptors.htm for documentation.

#ifndef STK_ITERATOR_ADAPTOR_GENERATOR_ITERATOR_HPP
#define STK_ITERATOR_ADAPTOR_GENERATOR_ITERATOR_HPP
#pragma once

#include <boost/iterator/iterator_facade.hpp>
#include <boost/utility/result_of.hpp>
#include <boost/ref.hpp>
#include <type_traits>

namespace stk {
	namespace iterators {

		template <class Generator>
		class generator_iterator
			: public boost::iterators::iterator_facade<
				  generator_iterator<Generator>,
				  decltype(std::declval<Generator>()()),
				  boost::iterators::single_pass_traversal_tag,
				  typename decltype(std::declval<Generator>()()) const&>
		{
			using result_type = decltype(std::declval<Generator>()());
			typedef boost::iterators::iterator_facade<
				generator_iterator<Generator>,
				result_type,
				boost::iterators::single_pass_traversal_tag,
				typename result_type const&>
				super_t;

		public:

			generator_iterator() = default;
			generator_iterator( Generator* g )
				: m_g( g )
				, m_value( ( *m_g )() )
			{}

			void increment()
			{
				m_value = ( *m_g )();
			}

			typename result_type const& dereference() const
			{
				return m_value;
			}

			bool equal( generator_iterator const& y ) const
			{
				return this->m_g == y.m_g && this->m_value == y.m_value;
			}

		private:
			Generator*  m_g;
			result_type m_value;
		};

		template <class Generator>
		struct generator_iterator_generator
		{
			typedef generator_iterator<Generator> type;
		};

		template <class Generator>
		inline generator_iterator<Generator>
		make_generator_iterator( Generator& gen )
		{
			typedef generator_iterator<Generator> result_t;
			return result_t( &gen );
		}

	} // namespace iterators

	using iterators::generator_iterator;
	using iterators::generator_iterator_generator;
	using iterators::make_generator_iterator;

} // namespace stk

#endif // STK_ITERATOR_ADAPTOR_GENERATOR_ITERATOR_HPP

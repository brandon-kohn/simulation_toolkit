//
//! Copyright © 2021
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <cstdint>
#include <boost/config.hpp>

namespace stk {
	namespace detail {
		template <typename Integer, typename EnableIf = void>
		struct hash_traits;

		template <typename T>
		struct hash_traits<T, typename std::enable_if<std::is_integral<T>::value && sizeof(T) == 4>::type>
		{
			BOOST_STATIC_CONSTEXPR std::uint32_t prime = 16777619u;
			BOOST_STATIC_CONSTEXPR std::uint32_t offset_basis = 2166136261u;
		};

		template <typename T>
		struct hash_traits<T, typename std::enable_if<std::is_integral<T>::value && sizeof(T) == 8>::type>
		{
			BOOST_STATIC_CONSTEXPR std::uint64_t prime = 1099511628211u;
			BOOST_STATIC_CONSTEXPR std::uint64_t offset_basis = 14695981039346656037u;
		};

	}//! namespace detail;

	template <typename HashType, typename Char>
	constexpr HashType fnv1a_hash( const Char* s ) BOOST_NOEXCEPT
	{
		auto hash = stk::detail::hash_traits<HashType>::offset_basis;

		while( *s != 0 )
			hash = ( hash ^ static_cast<HashType>( *( s++ ) ) ) * stk::detail::hash_traits<HashType>::prime;

		return hash;
	}

	constexpr std::size_t operator"" _hash( const char* s, std::size_t ) BOOST_NOEXCEPT
	{
		return fnv1a_hash<std::size_t>( s );
	}

	constexpr std::size_t operator"" _hash( const wchar_t* s, std::size_t ) BOOST_NOEXCEPT
	{
		return fnv1a_hash<std::size_t>( s );
	}

	constexpr std::uint32_t operator"" _hash32( const char* s, std::size_t ) BOOST_NOEXCEPT
	{
		return fnv1a_hash<std::uint32_t>( s );
	}

	constexpr std::uint32_t operator"" _hash32( const wchar_t* s, std::size_t ) BOOST_NOEXCEPT
	{
		return fnv1a_hash<std::uint32_t>( s );
	}

	constexpr std::uint64_t operator"" _hash64( const char* s, std::size_t ) BOOST_NOEXCEPT
	{
		return fnv1a_hash<std::uint64_t>( s );
	}

	constexpr std::uint64_t operator"" _hash64( const wchar_t* s, std::size_t ) BOOST_NOEXCEPT
	{
		return fnv1a_hash<std::uint64_t>( s );
	}

	template <typename HashedType = std::size_t, typename Char = char>
	class basic_string_hash
	{
	public:
		constexpr basic_string_hash( const Char* s ) BOOST_NOEXCEPT
			: m_string( s )
			, m_hash( fnv1a_hash<HashedType>( s ) )
		{}

		constexpr HashedType  hash() const { return m_hash; }
		constexpr const Char* key() const { return m_string; }
		constexpr bool        operator<( const basic_string_hash& rhs ) const { return m_hash < rhs.m_hash; }
		constexpr bool        operator==( const basic_string_hash& rhs ) const { return m_hash == rhs.m_hash; }

	private:
		const Char*      m_string;
		const HashedType m_hash;
	};

	template <typename HashType, typename Char>
	constexpr basic_string_hash<HashType, Char> make_string_hash( const Char* s )
	{
		return basic_string_hash<HashType, Char>{ s };
	}

	template <typename Char>
	constexpr basic_string_hash<std::uint32_t, Char> make_string_hash32( const Char* s )
	{
		return basic_string_hash<std::uint32_t, Char>{ s };
	}

	template <typename Char>
	constexpr basic_string_hash<std::uint64_t, Char> make_string_hash64( const Char* s )
	{
		return basic_string_hash<std::uint64_t, Char>{ s };
	}

	using string_hash = basic_string_hash<>;
	using string_hash32 = basic_string_hash<std::uint32_t>;
	using string_hash64 = basic_string_hash<std::uint64_t>;
	using wstring_hash = basic_string_hash<std::size_t, wchar_t>;
	using wstring_hash32 = basic_string_hash<std::uint32_t, wchar_t>;
	using wstring_hash64 = basic_string_hash<std::uint64_t, wchar_t>;

} // namespace stk

#include <boost/preprocessor/stringize.hpp>
#define STK_STRING_HASH( X ) stk::make_string_hash<std::size_t>( BOOST_PP_STRINGIZE(X) )
#define STK_STRING_HASH32( X ) stk::make_string_hash32( BOOST_PP_STRINGIZE(X) )
#define STK_STRING_HASH64( X ) stk::make_string_hash64( BOOST_PP_STRINGIZE(X) )

#include <boost/preprocessor/wstringize.hpp>
#define STK_WSTRING_HASH( X ) stk::make_wstring_hash<std::size_t>( BOOST_PP_WSTRINGIZE(X) )
#define STK_WSTRING_HASH32( X ) stk::make_wstring_hash32( BOOST_PP_WSTRINGIZE(X) )
#define STK_WSTRING_HASH64( X ) stk::make_wstring_hash64( BOOST_PP_WSTRINGIZE(X) )

namespace std {

	template <typename HashedType, typename Char>
	class hash<stk::basic_string_hash<HashedType, Char>>
	{
	public:
		HashedType operator()( const stk::basic_string_hash<HashedType, Char>& s ) const
		{
			return s.hash();
		}
	};

}//! namespace std;


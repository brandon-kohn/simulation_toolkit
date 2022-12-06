//
//! Copyright � 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_UTILITY_COMPRESSED_INTEGER_PAIR_HPP
#define STK_UTILITY_COMPRESSED_INTEGER_PAIR_HPP

#include <type_traits>

namespace stk {

    struct compressed_integer_pair
    {
        std::uint32_t first;
        std::uint32_t second;

        inline bool operator <(const compressed_integer_pair& rhs)
        {
            return to_uint64() < rhs.to_uint64();
        }

        BOOST_FORCEINLINE std::uint64_t to_uint64() const
        {
            //return *reinterpret_cast<std::uint64_t const*>(this);
            std::uint64_t least = second;
            std::uint64_t most = first;
            return most << 32 | least;
        }

        explicit operator bool() const
        {
            return to_uint64() != 0;
        }
    };

    static_assert(std::is_pod<compressed_integer_pair>::value, "compressed_integer_pair should be a POD of size 8 bytes.");
    static_assert(sizeof(compressed_integer_pair) == 8, "compressed_integer_pair should be a POD of size 8 bytes.");

    inline bool operator ==(const compressed_integer_pair& lhs, const compressed_integer_pair& rhs)
    {
        return lhs.to_uint64() == rhs.to_uint64();
    }

    inline bool operator !=(const compressed_integer_pair& lhs, const compressed_integer_pair& rhs)
    {
        return lhs.to_uint64() != rhs.to_uint64();
    }

}//! namespace stk;

namespace std
{
	template <>
	struct hash<stk::compressed_integer_pair>
	{
		std::size_t operator()(const stk::compressed_integer_pair& p) const
		{
			return std::hash<std::uint64_t>()(p.to_uint64());
		}
	};
}

#endif//! STK_UTILITY_COMPRESSED_INTEGER_PAIR_HPP


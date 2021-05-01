#include <cstdint>
#include <type_traits>
#include <geometrix/utility/assert.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <limits>

namespace stk {
	namespace detail {

		template <typename T>
		struct identity
		{
			using type = T;
		};

		template <typename T, typename Enable = void>
		struct make_unsigned
			: identity<T>
		{};

		template <typename T>
		struct make_unsigned<T, typename std::enable_if<std::is_integral<T>::value>::type>
			: std::make_unsigned<T>
		{};

		//! A policy selector to choose how to convert numeric types for comparison.
		template <typename T, typename U, typename EnableIf = void>
		struct safe_compare_cast_policy_selector
		{};

		//! A policy to convert using the implicit conversion rules specified by ansi.
		//! This happens when the types are the same, or both are signed, or both are unsigned, or either is floating point.
		struct use_ansi_conversion
		{};

		template <typename T, typename U>
		struct safe_compare_cast_policy_selector
		<
			T,
			U,
			typename std::enable_if
			<
			   std::is_same<typename std::decay<T>::type, typename std::decay<U>::type>::value 
			|| std::numeric_limits<T>::is_signed == std::numeric_limits<U>::is_signed 
			|| std::is_floating_point<T>::value || std::is_floating_point<U>::value
			>::type
		>
		{
			typedef use_ansi_conversion type;
		};

		//! A policy to convert to an unsigned type whose width is the same as T.
		//! This happens when one type is signed and the other is not and both are integral types, and size of U is less than size of T.
		struct convert_to_unsigned_T
		{};

		template <typename T, typename U>
		struct safe_compare_cast_policy_selector
		<
			T,
			U,
			typename std::enable_if
			<
			   std::numeric_limits<T>::is_signed != std::numeric_limits<U>::is_signed 
			&& std::is_integral<T>::value 
			&& std::is_integral<U>::value 
			&& sizeof( U ) < sizeof( T )
			>::type
		>
		{
			typedef convert_to_unsigned_T type;
		};

		//! A policy to convert to an unsigned type whose width is the same as U.
		//! This happens when one type is signed and the other is not and both are integral types, and size of U is >= size of T.
		struct convert_to_unsigned_U 
		{};

		template <typename T, typename U>
		struct safe_compare_cast_policy_selector<
			T,
			U,
			typename std::enable_if
			<
			   std::numeric_limits<T>::is_signed != std::numeric_limits<U>::is_signed
			&& std::is_integral<U>::value
			&& std::is_integral<T>::value
			&& sizeof( U ) >= sizeof( T )
			>::type
		>
		{
			typedef convert_to_unsigned_U type;
		};

		//! Same type or both signed or one is a floating point (let default casting deal with it) - return arg by ref
		template <typename T, typename U, typename Arg>
		inline Arg safe_compare_cast_impl( const Arg& u, use_ansi_conversion )
		{
			return u;
		}

		//! Not the same type, T is wider and is not signed, the other is.
		//! Safe to convert to T.
		template <typename T, typename U, typename Arg>
		inline typename make_unsigned<T>::type safe_compare_cast_impl( const Arg& u, convert_to_unsigned_T )
		{
			return static_cast<typename make_unsigned<T>::type>( u );
		}

		//! Not the same type, U is wider and is not signed, the other is.
		//! Safe to convert to U.
		template <typename T, typename U, typename Arg>
		inline typename make_unsigned<U>::type safe_compare_cast_impl( const Arg& u, convert_to_unsigned_U )
		{
			return static_cast<typename make_unsigned<U>::type>( u );
		}

		template <typename T, typename U, typename Arg>
		auto safe_compare_cast( Arg a ) -> decltype( safe_compare_cast_impl<T, U>( a, typename safe_compare_cast_policy_selector<T, U>::type() ) )
		{
			return safe_compare_cast_impl<T, U>( a, typename safe_compare_cast_policy_selector<T, U>::type() );
		}
	} // namespace detail

	//! A class to represent a range of numbers between 0 and numeric_limits<T>::max() - 1.
	//! The value for max T is reserved to mark an invalid unsigned value (-1).
	template <typename T>
	class unsigned_integer
	{
		template <typename U>
		friend class unsigned_integer;
		static_assert( boost::is_unsigned<T>::value, "unsigned_integer<T> type T must be unsigned." );

	public:
		static constexpr T invalid = static_cast<T>( -1 );

		unsigned_integer()
			: m_value( invalid )
		{}

		template <typename U>
		unsigned_integer( const unsigned_integer<U>& n )
			: m_value( n.is_valid() ? boost::numeric_cast<T>( n.m_value ) : invalid )
		{}

		//! Generate constructors for each fundamental numeric type.
		unsigned_integer( const unsigned char& n )
			: m_value( boost::numeric_cast<T>( n ) )
		{}

		unsigned_integer( const unsigned short& n )
			: m_value( boost::numeric_cast<T>( n ) )
		{}

		unsigned_integer( const unsigned long& n )
			: m_value( boost::numeric_cast<T>( n ) )
		{}

		unsigned_integer( const unsigned int& n )
			: m_value( boost::numeric_cast<T>( n ) )
		{}

		unsigned_integer( const unsigned long long& n )
			: m_value( boost::numeric_cast<T>( n ) )
		{}

		unsigned_integer( const bool& n )
			: m_value( boost::numeric_cast<T>( n ) )
		{}

		unsigned_integer( const char& n )
			: m_value( n < 0 ? invalid : boost::numeric_cast<T>( n ) )
		{}

		unsigned_integer( const signed char& n )
			: m_value( n < 0 ? invalid : boost::numeric_cast<T>( n ) )
		{}

		unsigned_integer( const short& n )
			: m_value( n < 0 ? invalid : boost::numeric_cast<T>( n ) )
		{}

		unsigned_integer( const long& n )
			: m_value( n < 0 ? invalid : boost::numeric_cast<T>( n ) )
		{}

		unsigned_integer( const int& n )
			: m_value( n < 0 ? invalid : boost::numeric_cast<T>( n ) )
		{}

		unsigned_integer( const long long& n )
			: m_value( n < 0 ? invalid : boost::numeric_cast<T>( n ) )
		{}

		unsigned_integer( const float& n )
			: m_value( n < 0 ? invalid : boost::numeric_cast<T>( n ) )
		{}

		unsigned_integer( const double& n )
			: m_value( n < 0 ? invalid : boost::numeric_cast<T>( n ) )
		{}

		unsigned_integer( const long double& n )
			: m_value( n < 0 ? invalid : boost::numeric_cast<T>( n ) )
		{}

		~unsigned_integer() = default;

		bool is_valid() const
		{
			return m_value != invalid;
		}

		bool is_invalid() const
		{
			return m_value == invalid;
		}

		unsigned_integer& operator=( const unsigned char& n )
		{
			m_value = boost::numeric_cast<T>( n );
			return *this;
		}

		unsigned_integer& operator=( const unsigned short& n )
		{
			m_value = boost::numeric_cast<T>( n );
			return *this;
		}

		unsigned_integer& operator=( const unsigned long& n )
		{
			m_value = boost::numeric_cast<T>( n );
			return *this;
		}

		unsigned_integer& operator=( const unsigned int& n )
		{
			m_value = boost::numeric_cast<T>( n );
			return *this;
		}

		unsigned_integer& operator=( const unsigned long long& n )
		{
			m_value = boost::numeric_cast<T>( n );
			return *this;
		}

		unsigned_integer& operator=( const bool& n )
		{
			m_value = boost::numeric_cast<T>( n );
			return *this;
		}

		unsigned_integer& operator=( const char& n )
		{
			m_value = n < 0 ? invalid : boost::numeric_cast<T>( n );
			return *this;
		}

		unsigned_integer& operator=( const signed char& n )
		{
			m_value = n < 0 ? invalid : boost::numeric_cast<T>( n );
			return *this;
		}

		unsigned_integer& operator=( const short& n )
		{
			m_value = n < 0 ? invalid : boost::numeric_cast<T>( n );
			return *this;
		}

		unsigned_integer& operator=( const long& n )
		{
			m_value = n < 0 ? invalid : boost::numeric_cast<T>( n );
			return *this;
		}

		unsigned_integer& operator=( const int& n )
		{
			m_value = n < 0 ? invalid : boost::numeric_cast<T>( n );
			return *this;
		}

		unsigned_integer& operator=( const long long& n )
		{
			m_value = n < 0 ? invalid : boost::numeric_cast<T>( n );
			return *this;
		}

		unsigned_integer& operator=( const float& n )
		{
			m_value = n < 0 ? invalid : boost::numeric_cast<T>( n );
			return *this;
		}

		unsigned_integer& operator=( const double& n )
		{
			m_value = n < 0 ? invalid : boost::numeric_cast<T>( n );
			return *this;
		}

		unsigned_integer& operator=( const long double& n )
		{
			m_value = n < 0 ? invalid : boost::numeric_cast<T>( n );
			return *this;
		}

		template <typename U>
		unsigned_integer& operator=( const unsigned_integer<U>& n )
		{
			m_value = ( n.is_valid() ? boost::numeric_cast<T>( n.m_value ) : invalid );
			return *this;
		}

		template <typename U>
		bool operator<( U rhs ) const
		{
			GEOMETRIX_ASSERT( is_valid() );
			if( rhs < 0 )
				return false;
			return detail::safe_compare_cast<T, U>( m_value ) < detail::safe_compare_cast<T, U>( rhs );
		}

		template <>
		bool operator<( const unsigned_integer<T>& rhs ) const
		{
			GEOMETRIX_ASSERT( is_valid() && rhs.is_valid() );
			return m_value < rhs.m_value;
		}

		template <typename U>
		bool operator>( U rhs ) const
		{
			GEOMETRIX_ASSERT( is_valid() );
			if( rhs < 0 )
				return true;
			return detail::safe_compare_cast<T, U>( m_value ) > detail::safe_compare_cast<T, U>( rhs );
		}

		template <>
		bool operator>( const unsigned_integer<T>& rhs ) const
		{
			GEOMETRIX_ASSERT( rhs.is_valid() );
			return m_value > rhs.m_value;
		}

		template <typename U>
		bool operator==( const U& rhs ) const
		{
			if( rhs < 0 )
				return rhs == U( -1 ) && is_invalid();

			return detail::safe_compare_cast<T, U>( m_value ) == detail::safe_compare_cast<T, U>( rhs );
		}

		template <>
		bool operator==( const unsigned_integer<T>& rhs ) const
		{
			return m_value == rhs.m_value;
		}

		bool operator==( bool rhs ) const
		{
			GEOMETRIX_ASSERT( is_valid() );
			return static_cast<bool>( m_value != 0 ) == rhs;
		}

		template <typename U>
		bool operator!=( const U& rhs ) const
		{
			return !operator==( rhs );
		}

		template <>
		bool operator!=( const unsigned_integer<T>& rhs ) const
		{
			return !operator==( rhs );
		}

		bool operator!=( bool rhs ) const
		{
			GEOMETRIX_ASSERT( is_valid() );
			return static_cast<bool>( m_value != 0 ) != rhs;
		}

		bool operator!() const
		{
			GEOMETRIX_ASSERT( is_valid() );
			return m_value == 0;
		}

		unsigned_integer<T>& operator++()
		{

			if( is_valid() )
				++m_value;
			return *this;
		}

		unsigned_integer<T>& operator--()
		{

			if( is_valid() )
				--m_value;
			return *this;
		}

		unsigned_integer<T>& operator+=( const unsigned_integer<T>& v )
		{
			return *this += v.m_value;
		}

		template <typename U>
		unsigned_integer<T>& operator+=( U v )
		{
			if( is_valid() )
			{
				T diff = ( std::numeric_limits<T>::max )() - m_value;
				if( ( v >= 0 && detail::safe_compare_cast<T, U>( v ) < detail::safe_compare_cast<T, U>( diff ) ) || ( v < 0 && detail::safe_compare_cast<T, U>( m_value ) >= detail::safe_compare_cast<T, U>( v ) ) )
					m_value += v;
				else
					m_value = invalid;
			}
			return *this;
		}

		unsigned_integer<T>& operator-=( const unsigned_integer<T>& v )
		{
			return *this -= v.m_value;
		}

		template <typename U>
		unsigned_integer<T>& operator-=( U v )
		{
			if( is_valid() )
			{
				T diff = ( std::numeric_limits<T>::max )() - m_value;
				if( ( v >= 0 && detail::safe_compare_cast<T, U>( m_value ) >= detail::safe_compare_cast<T, U>( v ) ) || ( v < 0 && detail::safe_compare_cast<T, U>( -v ) <= detail::safe_compare_cast<T, U>( diff ) ) )
					m_value -= v;
				else
					m_value = invalid;
			}
			return *this;
		}

		unsigned_integer<T>& operator*=( const unsigned_integer<T>& v )
		{
			return *this *= v.m_value;
		}

		template <typename U>
		unsigned_integer<T>& operator*=( U v )
		{
			if( is_valid() )
			{
				if( v < 0 )
					m_value = invalid;
				else if( v == 0 )
					m_value = 0;
				else if( detail::safe_compare_cast<T, U>( v ) < detail::safe_compare_cast<T, U>( ( std::numeric_limits<T>::max )() / v ) )
					m_value *= v;
				else
					m_value = invalid;
			}
			return ( *this );
		}

		unsigned_integer<T>& operator/=( const unsigned_integer<T>& v )
		{
			return *this /= v.m_value;
		}

		template <typename U>
		unsigned_integer<T>& operator/=( U v )
		{
			if( is_valid() )
			{
				if( v <= 0 )
					m_value = invalid;
				else
					m_value /= v;
			}
			return ( *this );
		}

		unsigned_integer<T>& operator%=( const unsigned_integer<T>& v )
		{
			return *this %= v.m_value;
		}

		template <typename U>
		unsigned_integer<T>& operator%=( U v )
		{
			if( is_valid() )
			{
				if( v < 0 )
					m_value = invalid;
				else
					m_value %= v;
			}
			return ( *this );
		}

		unsigned_integer<T>& operator^=( const unsigned_integer<T>& v )
		{
			return *this ^= v.m_value;
		}

		template <typename U>
		unsigned_integer<T>& operator^=( U v )
		{
			m_value ^= T( v );
			return ( *this );
		}

		unsigned_integer<T>& operator&=( const unsigned_integer<T>& v )
		{
			return *this &= v.m_value;
		}

		template <typename U>
		unsigned_integer<T>& operator&=( U v )
		{
			m_value &= T( v );
			return ( *this );
		}

		unsigned_integer<T>& operator|=( const unsigned_integer<T>& v )
		{
			return *this |= v.m_value;
		}

		template <typename U>
		unsigned_integer<T>& operator|=( U v )
		{
			m_value |= T( v );
			return ( *this );
		}

		T get_value() const { return m_value; }

		operator T() const
		{
			GEOMETRIX_ASSERT( is_valid() );
			return m_value;
		}

		explicit operator bool() const
		{
			GEOMETRIX_ASSERT( is_valid() );
			return m_value ? true : false;
		}

		friend bool                operator<=( const unsigned_integer<T>& x, const long double& y ) { return !static_cast<bool>( x > y ); }
		friend bool                operator>=( const unsigned_integer<T>& x, const long double& y ) { return !static_cast<bool>( x < y ); }
		friend bool                operator>( const long double& x, const unsigned_integer<T>& y ) { return y < x; }
		friend bool                operator<( const long double& x, const unsigned_integer<T>& y ) { return y > x; }
		friend bool                operator<=( const long double& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y < x ); }
		friend bool                operator>=( const long double& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y > x ); }
		friend bool                operator==( const long double& y, const unsigned_integer<T>& x ) { return x == y; }
		friend bool                operator!=( const long double& y, const unsigned_integer<T>& x ) { return !static_cast<bool>( x == y ); }
		friend bool                operator!=( const unsigned_integer<T>& y, const long double& x ) { return !static_cast<bool>( y == x ); }
		friend unsigned_integer<T> operator*( unsigned_integer<T> lhs, const long double& rhs ) { return lhs *= rhs; }
		friend unsigned_integer<T> operator*( const long double& lhs, unsigned_integer<T> rhs ) { return rhs *= lhs; }
		friend unsigned_integer<T> operator+( unsigned_integer<T> lhs, const long double& rhs ) { return lhs += rhs; }
		friend unsigned_integer<T> operator+( const long double& lhs, unsigned_integer<T> rhs ) { return rhs += lhs; }
		friend unsigned_integer<T> operator-( unsigned_integer<T> lhs, const long double& rhs ) { return lhs -= rhs; }
		friend unsigned_integer<T> operator-( const long double& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) -= rhs; }
		friend unsigned_integer<T> operator/( unsigned_integer<T> lhs, const long double& rhs ) { return lhs /= rhs; }
		friend unsigned_integer<T> operator/( const long double& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) /= rhs; }
		friend unsigned_integer<T> operator%( unsigned_integer<T> lhs, const long double& rhs ) { return lhs %= rhs; }
		friend unsigned_integer<T> operator%( const long double& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) %= rhs; }
		friend unsigned_integer<T> operator^( unsigned_integer<T> lhs, const long double& rhs ) { return lhs ^= rhs; }
		friend unsigned_integer<T> operator^( const long double& lhs, unsigned_integer<T> rhs ) { return rhs ^= lhs; }
		friend unsigned_integer<T> operator&( unsigned_integer<T> lhs, const long double& rhs ) { return lhs &= rhs; }
		friend unsigned_integer<T> operator&( const long double& lhs, unsigned_integer<T> rhs ) { return rhs &= lhs; }
		friend unsigned_integer<T> operator|( unsigned_integer<T> lhs, const long double& rhs ) { return lhs |= rhs; }
		friend unsigned_integer<T> operator|( const long double& lhs, unsigned_integer<T> rhs ) { return rhs |= lhs; };
		friend bool                operator<=( const unsigned_integer<T>& x, const double& y ) { return !static_cast<bool>( x > y ); }
		friend bool                operator>=( const unsigned_integer<T>& x, const double& y ) { return !static_cast<bool>( x < y ); }
		friend bool                operator>( const double& x, const unsigned_integer<T>& y ) { return y < x; }
		friend bool                operator<( const double& x, const unsigned_integer<T>& y ) { return y > x; }
		friend bool                operator<=( const double& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y < x ); }
		friend bool                operator>=( const double& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y > x ); }
		friend bool                operator==( const double& y, const unsigned_integer<T>& x ) { return x == y; }
		friend bool                operator!=( const double& y, const unsigned_integer<T>& x ) { return !static_cast<bool>( x == y ); }
		friend bool                operator!=( const unsigned_integer<T>& y, const double& x ) { return !static_cast<bool>( y == x ); }
		friend unsigned_integer<T> operator*( unsigned_integer<T> lhs, const double& rhs ) { return lhs *= rhs; }
		friend unsigned_integer<T> operator*( const double& lhs, unsigned_integer<T> rhs ) { return rhs *= lhs; }
		friend unsigned_integer<T> operator+( unsigned_integer<T> lhs, const double& rhs ) { return lhs += rhs; }
		friend unsigned_integer<T> operator+( const double& lhs, unsigned_integer<T> rhs ) { return rhs += lhs; }
		friend unsigned_integer<T> operator-( unsigned_integer<T> lhs, const double& rhs ) { return lhs -= rhs; }
		friend unsigned_integer<T> operator-( const double& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) -= rhs; }
		friend unsigned_integer<T> operator/( unsigned_integer<T> lhs, const double& rhs ) { return lhs /= rhs; }
		friend unsigned_integer<T> operator/( const double& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) /= rhs; }
		friend unsigned_integer<T> operator%( unsigned_integer<T> lhs, const double& rhs ) { return lhs %= rhs; }
		friend unsigned_integer<T> operator%( const double& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) %= rhs; }
		friend unsigned_integer<T> operator^( unsigned_integer<T> lhs, const double& rhs ) { return lhs ^= rhs; }
		friend unsigned_integer<T> operator^( const double& lhs, unsigned_integer<T> rhs ) { return rhs ^= lhs; }
		friend unsigned_integer<T> operator&( unsigned_integer<T> lhs, const double& rhs ) { return lhs &= rhs; }
		friend unsigned_integer<T> operator&( const double& lhs, unsigned_integer<T> rhs ) { return rhs &= lhs; }
		friend unsigned_integer<T> operator|( unsigned_integer<T> lhs, const double& rhs ) { return lhs |= rhs; }
		friend unsigned_integer<T> operator|( const double& lhs, unsigned_integer<T> rhs ) { return rhs |= lhs; };
		friend bool                operator<=( const unsigned_integer<T>& x, const float& y ) { return !static_cast<bool>( x > y ); }
		friend bool                operator>=( const unsigned_integer<T>& x, const float& y ) { return !static_cast<bool>( x < y ); }
		friend bool                operator>( const float& x, const unsigned_integer<T>& y ) { return y < x; }
		friend bool                operator<( const float& x, const unsigned_integer<T>& y ) { return y > x; }
		friend bool                operator<=( const float& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y < x ); }
		friend bool                operator>=( const float& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y > x ); }
		friend bool                operator==( const float& y, const unsigned_integer<T>& x ) { return x == y; }
		friend bool                operator!=( const float& y, const unsigned_integer<T>& x ) { return !static_cast<bool>( x == y ); }
		friend bool                operator!=( const unsigned_integer<T>& y, const float& x ) { return !static_cast<bool>( y == x ); }
		friend unsigned_integer<T> operator*( unsigned_integer<T> lhs, const float& rhs ) { return lhs *= rhs; }
		friend unsigned_integer<T> operator*( const float& lhs, unsigned_integer<T> rhs ) { return rhs *= lhs; }
		friend unsigned_integer<T> operator+( unsigned_integer<T> lhs, const float& rhs ) { return lhs += rhs; }
		friend unsigned_integer<T> operator+( const float& lhs, unsigned_integer<T> rhs ) { return rhs += lhs; }
		friend unsigned_integer<T> operator-( unsigned_integer<T> lhs, const float& rhs ) { return lhs -= rhs; }
		friend unsigned_integer<T> operator-( const float& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) -= rhs; }
		friend unsigned_integer<T> operator/( unsigned_integer<T> lhs, const float& rhs ) { return lhs /= rhs; }
		friend unsigned_integer<T> operator/( const float& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) /= rhs; }
		friend unsigned_integer<T> operator%( unsigned_integer<T> lhs, const float& rhs ) { return lhs %= rhs; }
		friend unsigned_integer<T> operator%( const float& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) %= rhs; }
		friend unsigned_integer<T> operator^( unsigned_integer<T> lhs, const float& rhs ) { return lhs ^= rhs; }
		friend unsigned_integer<T> operator^( const float& lhs, unsigned_integer<T> rhs ) { return rhs ^= lhs; }
		friend unsigned_integer<T> operator&( unsigned_integer<T> lhs, const float& rhs ) { return lhs &= rhs; }
		friend unsigned_integer<T> operator&( const float& lhs, unsigned_integer<T> rhs ) { return rhs &= lhs; }
		friend unsigned_integer<T> operator|( unsigned_integer<T> lhs, const float& rhs ) { return lhs |= rhs; }
		friend unsigned_integer<T> operator|( const float& lhs, unsigned_integer<T> rhs ) { return rhs |= lhs; };
		friend bool                operator<=( const unsigned_integer<T>& x, const char& y ) { return !static_cast<bool>( x > y ); }
		friend bool                operator>=( const unsigned_integer<T>& x, const char& y ) { return !static_cast<bool>( x < y ); }
		friend bool                operator>( const char& x, const unsigned_integer<T>& y ) { return y < x; }
		friend bool                operator<( const char& x, const unsigned_integer<T>& y ) { return y > x; }
		friend bool                operator<=( const char& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y < x ); }
		friend bool                operator>=( const char& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y > x ); }
		friend bool                operator==( const char& y, const unsigned_integer<T>& x ) { return x == y; }
		friend bool                operator!=( const char& y, const unsigned_integer<T>& x ) { return !static_cast<bool>( x == y ); }
		friend bool                operator!=( const unsigned_integer<T>& y, const char& x ) { return !static_cast<bool>( y == x ); }
		friend unsigned_integer<T> operator*( unsigned_integer<T> lhs, const char& rhs ) { return lhs *= rhs; }
		friend unsigned_integer<T> operator*( const char& lhs, unsigned_integer<T> rhs ) { return rhs *= lhs; }
		friend unsigned_integer<T> operator+( unsigned_integer<T> lhs, const char& rhs ) { return lhs += rhs; }
		friend unsigned_integer<T> operator+( const char& lhs, unsigned_integer<T> rhs ) { return rhs += lhs; }
		friend unsigned_integer<T> operator-( unsigned_integer<T> lhs, const char& rhs ) { return lhs -= rhs; }
		friend unsigned_integer<T> operator-( const char& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) -= rhs; }
		friend unsigned_integer<T> operator/( unsigned_integer<T> lhs, const char& rhs ) { return lhs /= rhs; }
		friend unsigned_integer<T> operator/( const char& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) /= rhs; }
		friend unsigned_integer<T> operator%( unsigned_integer<T> lhs, const char& rhs ) { return lhs %= rhs; }
		friend unsigned_integer<T> operator%( const char& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) %= rhs; }
		friend unsigned_integer<T> operator^( unsigned_integer<T> lhs, const char& rhs ) { return lhs ^= rhs; }
		friend unsigned_integer<T> operator^( const char& lhs, unsigned_integer<T> rhs ) { return rhs ^= lhs; }
		friend unsigned_integer<T> operator&( unsigned_integer<T> lhs, const char& rhs ) { return lhs &= rhs; }
		friend unsigned_integer<T> operator&( const char& lhs, unsigned_integer<T> rhs ) { return rhs &= lhs; }
		friend unsigned_integer<T> operator|( unsigned_integer<T> lhs, const char& rhs ) { return lhs |= rhs; }
		friend unsigned_integer<T> operator|( const char& lhs, unsigned_integer<T> rhs ) { return rhs |= lhs; };
		friend bool                operator<=( const unsigned_integer<T>& x, const signed char& y ) { return !static_cast<bool>( x > y ); }
		friend bool                operator>=( const unsigned_integer<T>& x, const signed char& y ) { return !static_cast<bool>( x < y ); }
		friend bool                operator>( const signed char& x, const unsigned_integer<T>& y ) { return y < x; }
		friend bool                operator<( const signed char& x, const unsigned_integer<T>& y ) { return y > x; }
		friend bool                operator<=( const signed char& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y < x ); }
		friend bool                operator>=( const signed char& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y > x ); }
		friend bool                operator==( const signed char& y, const unsigned_integer<T>& x ) { return x == y; }
		friend bool                operator!=( const signed char& y, const unsigned_integer<T>& x ) { return !static_cast<bool>( x == y ); }
		friend bool                operator!=( const unsigned_integer<T>& y, const signed char& x ) { return !static_cast<bool>( y == x ); }
		friend unsigned_integer<T> operator*( unsigned_integer<T> lhs, const signed char& rhs ) { return lhs *= rhs; }
		friend unsigned_integer<T> operator*( const signed char& lhs, unsigned_integer<T> rhs ) { return rhs *= lhs; }
		friend unsigned_integer<T> operator+( unsigned_integer<T> lhs, const signed char& rhs ) { return lhs += rhs; }
		friend unsigned_integer<T> operator+( const signed char& lhs, unsigned_integer<T> rhs ) { return rhs += lhs; }
		friend unsigned_integer<T> operator-( unsigned_integer<T> lhs, const signed char& rhs ) { return lhs -= rhs; }
		friend unsigned_integer<T> operator-( const signed char& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) -= rhs; }
		friend unsigned_integer<T> operator/( unsigned_integer<T> lhs, const signed char& rhs ) { return lhs /= rhs; }
		friend unsigned_integer<T> operator/( const signed char& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) /= rhs; }
		friend unsigned_integer<T> operator%( unsigned_integer<T> lhs, const signed char& rhs ) { return lhs %= rhs; }
		friend unsigned_integer<T> operator%( const signed char& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) %= rhs; }
		friend unsigned_integer<T> operator^( unsigned_integer<T> lhs, const signed char& rhs ) { return lhs ^= rhs; }
		friend unsigned_integer<T> operator^( const signed char& lhs, unsigned_integer<T> rhs ) { return rhs ^= lhs; }
		friend unsigned_integer<T> operator&( unsigned_integer<T> lhs, const signed char& rhs ) { return lhs &= rhs; }
		friend unsigned_integer<T> operator&( const signed char& lhs, unsigned_integer<T> rhs ) { return rhs &= lhs; }
		friend unsigned_integer<T> operator|( unsigned_integer<T> lhs, const signed char& rhs ) { return lhs |= rhs; }
		friend unsigned_integer<T> operator|( const signed char& lhs, unsigned_integer<T> rhs ) { return rhs |= lhs; };
		friend bool                operator<=( const unsigned_integer<T>& x, const unsigned char& y ) { return !static_cast<bool>( x > y ); }
		friend bool                operator>=( const unsigned_integer<T>& x, const unsigned char& y ) { return !static_cast<bool>( x < y ); }
		friend bool                operator>( const unsigned char& x, const unsigned_integer<T>& y ) { return y < x; }
		friend bool                operator<( const unsigned char& x, const unsigned_integer<T>& y ) { return y > x; }
		friend bool                operator<=( const unsigned char& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y < x ); }
		friend bool                operator>=( const unsigned char& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y > x ); }
		friend bool                operator==( const unsigned char& y, const unsigned_integer<T>& x ) { return x == y; }
		friend bool                operator!=( const unsigned char& y, const unsigned_integer<T>& x ) { return !static_cast<bool>( x == y ); }
		friend bool                operator!=( const unsigned_integer<T>& y, const unsigned char& x ) { return !static_cast<bool>( y == x ); }
		friend unsigned_integer<T> operator*( unsigned_integer<T> lhs, const unsigned char& rhs ) { return lhs *= rhs; }
		friend unsigned_integer<T> operator*( const unsigned char& lhs, unsigned_integer<T> rhs ) { return rhs *= lhs; }
		friend unsigned_integer<T> operator+( unsigned_integer<T> lhs, const unsigned char& rhs ) { return lhs += rhs; }
		friend unsigned_integer<T> operator+( const unsigned char& lhs, unsigned_integer<T> rhs ) { return rhs += lhs; }
		friend unsigned_integer<T> operator-( unsigned_integer<T> lhs, const unsigned char& rhs ) { return lhs -= rhs; }
		friend unsigned_integer<T> operator-( const unsigned char& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) -= rhs; }
		friend unsigned_integer<T> operator/( unsigned_integer<T> lhs, const unsigned char& rhs ) { return lhs /= rhs; }
		friend unsigned_integer<T> operator/( const unsigned char& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) /= rhs; }
		friend unsigned_integer<T> operator%( unsigned_integer<T> lhs, const unsigned char& rhs ) { return lhs %= rhs; }
		friend unsigned_integer<T> operator%( const unsigned char& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) %= rhs; }
		friend unsigned_integer<T> operator^( unsigned_integer<T> lhs, const unsigned char& rhs ) { return lhs ^= rhs; }
		friend unsigned_integer<T> operator^( const unsigned char& lhs, unsigned_integer<T> rhs ) { return rhs ^= lhs; }
		friend unsigned_integer<T> operator&( unsigned_integer<T> lhs, const unsigned char& rhs ) { return lhs &= rhs; }
		friend unsigned_integer<T> operator&( const unsigned char& lhs, unsigned_integer<T> rhs ) { return rhs &= lhs; }
		friend unsigned_integer<T> operator|( unsigned_integer<T> lhs, const unsigned char& rhs ) { return lhs |= rhs; }
		friend unsigned_integer<T> operator|( const unsigned char& lhs, unsigned_integer<T> rhs ) { return rhs |= lhs; };
		friend bool                operator<=( const unsigned_integer<T>& x, const short& y ) { return !static_cast<bool>( x > y ); }
		friend bool                operator>=( const unsigned_integer<T>& x, const short& y ) { return !static_cast<bool>( x < y ); }
		friend bool                operator>( const short& x, const unsigned_integer<T>& y ) { return y < x; }
		friend bool                operator<( const short& x, const unsigned_integer<T>& y ) { return y > x; }
		friend bool                operator<=( const short& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y < x ); }
		friend bool                operator>=( const short& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y > x ); }
		friend bool                operator==( const short& y, const unsigned_integer<T>& x ) { return x == y; }
		friend bool                operator!=( const short& y, const unsigned_integer<T>& x ) { return !static_cast<bool>( x == y ); }
		friend bool                operator!=( const unsigned_integer<T>& y, const short& x ) { return !static_cast<bool>( y == x ); }
		friend unsigned_integer<T> operator*( unsigned_integer<T> lhs, const short& rhs ) { return lhs *= rhs; }
		friend unsigned_integer<T> operator*( const short& lhs, unsigned_integer<T> rhs ) { return rhs *= lhs; }
		friend unsigned_integer<T> operator+( unsigned_integer<T> lhs, const short& rhs ) { return lhs += rhs; }
		friend unsigned_integer<T> operator+( const short& lhs, unsigned_integer<T> rhs ) { return rhs += lhs; }
		friend unsigned_integer<T> operator-( unsigned_integer<T> lhs, const short& rhs ) { return lhs -= rhs; }
		friend unsigned_integer<T> operator-( const short& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) -= rhs; }
		friend unsigned_integer<T> operator/( unsigned_integer<T> lhs, const short& rhs ) { return lhs /= rhs; }
		friend unsigned_integer<T> operator/( const short& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) /= rhs; }
		friend unsigned_integer<T> operator%( unsigned_integer<T> lhs, const short& rhs ) { return lhs %= rhs; }
		friend unsigned_integer<T> operator%( const short& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) %= rhs; }
		friend unsigned_integer<T> operator^( unsigned_integer<T> lhs, const short& rhs ) { return lhs ^= rhs; }
		friend unsigned_integer<T> operator^( const short& lhs, unsigned_integer<T> rhs ) { return rhs ^= lhs; }
		friend unsigned_integer<T> operator&( unsigned_integer<T> lhs, const short& rhs ) { return lhs &= rhs; }
		friend unsigned_integer<T> operator&( const short& lhs, unsigned_integer<T> rhs ) { return rhs &= lhs; }
		friend unsigned_integer<T> operator|( unsigned_integer<T> lhs, const short& rhs ) { return lhs |= rhs; }
		friend unsigned_integer<T> operator|( const short& lhs, unsigned_integer<T> rhs ) { return rhs |= lhs; };
		friend bool                operator<=( const unsigned_integer<T>& x, const unsigned short& y ) { return !static_cast<bool>( x > y ); }
		friend bool                operator>=( const unsigned_integer<T>& x, const unsigned short& y ) { return !static_cast<bool>( x < y ); }
		friend bool                operator>( const unsigned short& x, const unsigned_integer<T>& y ) { return y < x; }
		friend bool                operator<( const unsigned short& x, const unsigned_integer<T>& y ) { return y > x; }
		friend bool                operator<=( const unsigned short& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y < x ); }
		friend bool                operator>=( const unsigned short& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y > x ); }
		friend bool                operator==( const unsigned short& y, const unsigned_integer<T>& x ) { return x == y; }
		friend bool                operator!=( const unsigned short& y, const unsigned_integer<T>& x ) { return !static_cast<bool>( x == y ); }
		friend bool                operator!=( const unsigned_integer<T>& y, const unsigned short& x ) { return !static_cast<bool>( y == x ); }
		friend unsigned_integer<T> operator*( unsigned_integer<T> lhs, const unsigned short& rhs ) { return lhs *= rhs; }
		friend unsigned_integer<T> operator*( const unsigned short& lhs, unsigned_integer<T> rhs ) { return rhs *= lhs; }
		friend unsigned_integer<T> operator+( unsigned_integer<T> lhs, const unsigned short& rhs ) { return lhs += rhs; }
		friend unsigned_integer<T> operator+( const unsigned short& lhs, unsigned_integer<T> rhs ) { return rhs += lhs; }
		friend unsigned_integer<T> operator-( unsigned_integer<T> lhs, const unsigned short& rhs ) { return lhs -= rhs; }
		friend unsigned_integer<T> operator-( const unsigned short& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) -= rhs; }
		friend unsigned_integer<T> operator/( unsigned_integer<T> lhs, const unsigned short& rhs ) { return lhs /= rhs; }
		friend unsigned_integer<T> operator/( const unsigned short& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) /= rhs; }
		friend unsigned_integer<T> operator%( unsigned_integer<T> lhs, const unsigned short& rhs ) { return lhs %= rhs; }
		friend unsigned_integer<T> operator%( const unsigned short& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) %= rhs; }
		friend unsigned_integer<T> operator^( unsigned_integer<T> lhs, const unsigned short& rhs ) { return lhs ^= rhs; }
		friend unsigned_integer<T> operator^( const unsigned short& lhs, unsigned_integer<T> rhs ) { return rhs ^= lhs; }
		friend unsigned_integer<T> operator&( unsigned_integer<T> lhs, const unsigned short& rhs ) { return lhs &= rhs; }
		friend unsigned_integer<T> operator&( const unsigned short& lhs, unsigned_integer<T> rhs ) { return rhs &= lhs; }
		friend unsigned_integer<T> operator|( unsigned_integer<T> lhs, const unsigned short& rhs ) { return lhs |= rhs; }
		friend unsigned_integer<T> operator|( const unsigned short& lhs, unsigned_integer<T> rhs ) { return rhs |= lhs; };
		friend bool                operator<=( const unsigned_integer<T>& x, const int& y ) { return !static_cast<bool>( x > y ); }
		friend bool                operator>=( const unsigned_integer<T>& x, const int& y ) { return !static_cast<bool>( x < y ); }
		friend bool                operator>( const int& x, const unsigned_integer<T>& y ) { return y < x; }
		friend bool                operator<( const int& x, const unsigned_integer<T>& y ) { return y > x; }
		friend bool                operator<=( const int& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y < x ); }
		friend bool                operator>=( const int& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y > x ); }
		friend bool                operator==( const int& y, const unsigned_integer<T>& x ) { return x == y; }
		friend bool                operator!=( const int& y, const unsigned_integer<T>& x ) { return !static_cast<bool>( x == y ); }
		friend bool                operator!=( const unsigned_integer<T>& y, const int& x ) { return !static_cast<bool>( y == x ); }
		friend unsigned_integer<T> operator*( unsigned_integer<T> lhs, const int& rhs ) { return lhs *= rhs; }
		friend unsigned_integer<T> operator*( const int& lhs, unsigned_integer<T> rhs ) { return rhs *= lhs; }
		friend unsigned_integer<T> operator+( unsigned_integer<T> lhs, const int& rhs ) { return lhs += rhs; }
		friend unsigned_integer<T> operator+( const int& lhs, unsigned_integer<T> rhs ) { return rhs += lhs; }
		friend unsigned_integer<T> operator-( unsigned_integer<T> lhs, const int& rhs ) { return lhs -= rhs; }
		friend unsigned_integer<T> operator-( const int& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) -= rhs; }
		friend unsigned_integer<T> operator/( unsigned_integer<T> lhs, const int& rhs ) { return lhs /= rhs; }
		friend unsigned_integer<T> operator/( const int& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) /= rhs; }
		friend unsigned_integer<T> operator%( unsigned_integer<T> lhs, const int& rhs ) { return lhs %= rhs; }
		friend unsigned_integer<T> operator%( const int& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) %= rhs; }
		friend unsigned_integer<T> operator^( unsigned_integer<T> lhs, const int& rhs ) { return lhs ^= rhs; }
		friend unsigned_integer<T> operator^( const int& lhs, unsigned_integer<T> rhs ) { return rhs ^= lhs; }
		friend unsigned_integer<T> operator&( unsigned_integer<T> lhs, const int& rhs ) { return lhs &= rhs; }
		friend unsigned_integer<T> operator&( const int& lhs, unsigned_integer<T> rhs ) { return rhs &= lhs; }
		friend unsigned_integer<T> operator|( unsigned_integer<T> lhs, const int& rhs ) { return lhs |= rhs; }
		friend unsigned_integer<T> operator|( const int& lhs, unsigned_integer<T> rhs ) { return rhs |= lhs; };
		friend bool                operator<=( const unsigned_integer<T>& x, const unsigned int& y ) { return !static_cast<bool>( x > y ); }
		friend bool                operator>=( const unsigned_integer<T>& x, const unsigned int& y ) { return !static_cast<bool>( x < y ); }
		friend bool                operator>( const unsigned int& x, const unsigned_integer<T>& y ) { return y < x; }
		friend bool                operator<( const unsigned int& x, const unsigned_integer<T>& y ) { return y > x; }
		friend bool                operator<=( const unsigned int& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y < x ); }
		friend bool                operator>=( const unsigned int& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y > x ); }
		friend bool                operator==( const unsigned int& y, const unsigned_integer<T>& x ) { return x == y; }
		friend bool                operator!=( const unsigned int& y, const unsigned_integer<T>& x ) { return !static_cast<bool>( x == y ); }
		friend bool                operator!=( const unsigned_integer<T>& y, const unsigned int& x ) { return !static_cast<bool>( y == x ); }
		friend unsigned_integer<T> operator*( unsigned_integer<T> lhs, const unsigned int& rhs ) { return lhs *= rhs; }
		friend unsigned_integer<T> operator*( const unsigned int& lhs, unsigned_integer<T> rhs ) { return rhs *= lhs; }
		friend unsigned_integer<T> operator+( unsigned_integer<T> lhs, const unsigned int& rhs ) { return lhs += rhs; }
		friend unsigned_integer<T> operator+( const unsigned int& lhs, unsigned_integer<T> rhs ) { return rhs += lhs; }
		friend unsigned_integer<T> operator-( unsigned_integer<T> lhs, const unsigned int& rhs ) { return lhs -= rhs; }
		friend unsigned_integer<T> operator-( const unsigned int& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) -= rhs; }
		friend unsigned_integer<T> operator/( unsigned_integer<T> lhs, const unsigned int& rhs ) { return lhs /= rhs; }
		friend unsigned_integer<T> operator/( const unsigned int& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) /= rhs; }
		friend unsigned_integer<T> operator%( unsigned_integer<T> lhs, const unsigned int& rhs ) { return lhs %= rhs; }
		friend unsigned_integer<T> operator%( const unsigned int& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) %= rhs; }
		friend unsigned_integer<T> operator^( unsigned_integer<T> lhs, const unsigned int& rhs ) { return lhs ^= rhs; }
		friend unsigned_integer<T> operator^( const unsigned int& lhs, unsigned_integer<T> rhs ) { return rhs ^= lhs; }
		friend unsigned_integer<T> operator&( unsigned_integer<T> lhs, const unsigned int& rhs ) { return lhs &= rhs; }
		friend unsigned_integer<T> operator&( const unsigned int& lhs, unsigned_integer<T> rhs ) { return rhs &= lhs; }
		friend unsigned_integer<T> operator|( unsigned_integer<T> lhs, const unsigned int& rhs ) { return lhs |= rhs; }
		friend unsigned_integer<T> operator|( const unsigned int& lhs, unsigned_integer<T> rhs ) { return rhs |= lhs; };
		friend bool                operator<=( const unsigned_integer<T>& x, const long& y ) { return !static_cast<bool>( x > y ); }
		friend bool                operator>=( const unsigned_integer<T>& x, const long& y ) { return !static_cast<bool>( x < y ); }
		friend bool                operator>( const long& x, const unsigned_integer<T>& y ) { return y < x; }
		friend bool                operator<( const long& x, const unsigned_integer<T>& y ) { return y > x; }
		friend bool                operator<=( const long& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y < x ); }
		friend bool                operator>=( const long& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y > x ); }
		friend bool                operator==( const long& y, const unsigned_integer<T>& x ) { return x == y; }
		friend bool                operator!=( const long& y, const unsigned_integer<T>& x ) { return !static_cast<bool>( x == y ); }
		friend bool                operator!=( const unsigned_integer<T>& y, const long& x ) { return !static_cast<bool>( y == x ); }
		friend unsigned_integer<T> operator*( unsigned_integer<T> lhs, const long& rhs ) { return lhs *= rhs; }
		friend unsigned_integer<T> operator*( const long& lhs, unsigned_integer<T> rhs ) { return rhs *= lhs; }
		friend unsigned_integer<T> operator+( unsigned_integer<T> lhs, const long& rhs ) { return lhs += rhs; }
		friend unsigned_integer<T> operator+( const long& lhs, unsigned_integer<T> rhs ) { return rhs += lhs; }
		friend unsigned_integer<T> operator-( unsigned_integer<T> lhs, const long& rhs ) { return lhs -= rhs; }
		friend unsigned_integer<T> operator-( const long& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) -= rhs; }
		friend unsigned_integer<T> operator/( unsigned_integer<T> lhs, const long& rhs ) { return lhs /= rhs; }
		friend unsigned_integer<T> operator/( const long& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) /= rhs; }
		friend unsigned_integer<T> operator%( unsigned_integer<T> lhs, const long& rhs ) { return lhs %= rhs; }
		friend unsigned_integer<T> operator%( const long& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) %= rhs; }
		friend unsigned_integer<T> operator^( unsigned_integer<T> lhs, const long& rhs ) { return lhs ^= rhs; }
		friend unsigned_integer<T> operator^( const long& lhs, unsigned_integer<T> rhs ) { return rhs ^= lhs; }
		friend unsigned_integer<T> operator&( unsigned_integer<T> lhs, const long& rhs ) { return lhs &= rhs; }
		friend unsigned_integer<T> operator&( const long& lhs, unsigned_integer<T> rhs ) { return rhs &= lhs; }
		friend unsigned_integer<T> operator|( unsigned_integer<T> lhs, const long& rhs ) { return lhs |= rhs; }
		friend unsigned_integer<T> operator|( const long& lhs, unsigned_integer<T> rhs ) { return rhs |= lhs; };
		friend bool                operator<=( const unsigned_integer<T>& x, const unsigned long& y ) { return !static_cast<bool>( x > y ); }
		friend bool                operator>=( const unsigned_integer<T>& x, const unsigned long& y ) { return !static_cast<bool>( x < y ); }
		friend bool                operator>( const unsigned long& x, const unsigned_integer<T>& y ) { return y < x; }
		friend bool                operator<( const unsigned long& x, const unsigned_integer<T>& y ) { return y > x; }
		friend bool                operator<=( const unsigned long& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y < x ); }
		friend bool                operator>=( const unsigned long& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y > x ); }
		friend bool                operator==( const unsigned long& y, const unsigned_integer<T>& x ) { return x == y; }
		friend bool                operator!=( const unsigned long& y, const unsigned_integer<T>& x ) { return !static_cast<bool>( x == y ); }
		friend bool                operator!=( const unsigned_integer<T>& y, const unsigned long& x ) { return !static_cast<bool>( y == x ); }
		friend unsigned_integer<T> operator*( unsigned_integer<T> lhs, const unsigned long& rhs ) { return lhs *= rhs; }
		friend unsigned_integer<T> operator*( const unsigned long& lhs, unsigned_integer<T> rhs ) { return rhs *= lhs; }
		friend unsigned_integer<T> operator+( unsigned_integer<T> lhs, const unsigned long& rhs ) { return lhs += rhs; }
		friend unsigned_integer<T> operator+( const unsigned long& lhs, unsigned_integer<T> rhs ) { return rhs += lhs; }
		friend unsigned_integer<T> operator-( unsigned_integer<T> lhs, const unsigned long& rhs ) { return lhs -= rhs; }
		friend unsigned_integer<T> operator-( const unsigned long& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) -= rhs; }
		friend unsigned_integer<T> operator/( unsigned_integer<T> lhs, const unsigned long& rhs ) { return lhs /= rhs; }
		friend unsigned_integer<T> operator/( const unsigned long& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) /= rhs; }
		friend unsigned_integer<T> operator%( unsigned_integer<T> lhs, const unsigned long& rhs ) { return lhs %= rhs; }
		friend unsigned_integer<T> operator%( const unsigned long& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) %= rhs; }
		friend unsigned_integer<T> operator^( unsigned_integer<T> lhs, const unsigned long& rhs ) { return lhs ^= rhs; }
		friend unsigned_integer<T> operator^( const unsigned long& lhs, unsigned_integer<T> rhs ) { return rhs ^= lhs; }
		friend unsigned_integer<T> operator&( unsigned_integer<T> lhs, const unsigned long& rhs ) { return lhs &= rhs; }
		friend unsigned_integer<T> operator&( const unsigned long& lhs, unsigned_integer<T> rhs ) { return rhs &= lhs; }
		friend unsigned_integer<T> operator|( unsigned_integer<T> lhs, const unsigned long& rhs ) { return lhs |= rhs; }
		friend unsigned_integer<T> operator|( const unsigned long& lhs, unsigned_integer<T> rhs ) { return rhs |= lhs; };
		friend bool                operator<=( const unsigned_integer<T>& x, const long long& y ) { return !static_cast<bool>( x > y ); }
		friend bool                operator>=( const unsigned_integer<T>& x, const long long& y ) { return !static_cast<bool>( x < y ); }
		friend bool                operator>( const long long& x, const unsigned_integer<T>& y ) { return y < x; }
		friend bool                operator<( const long long& x, const unsigned_integer<T>& y ) { return y > x; }
		friend bool                operator<=( const long long& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y < x ); }
		friend bool                operator>=( const long long& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y > x ); }
		friend bool                operator==( const long long& y, const unsigned_integer<T>& x ) { return x == y; }
		friend bool                operator!=( const long long& y, const unsigned_integer<T>& x ) { return !static_cast<bool>( x == y ); }
		friend bool                operator!=( const unsigned_integer<T>& y, const long long& x ) { return !static_cast<bool>( y == x ); }
		friend unsigned_integer<T> operator*( unsigned_integer<T> lhs, const long long& rhs ) { return lhs *= rhs; }
		friend unsigned_integer<T> operator*( const long long& lhs, unsigned_integer<T> rhs ) { return rhs *= lhs; }
		friend unsigned_integer<T> operator+( unsigned_integer<T> lhs, const long long& rhs ) { return lhs += rhs; }
		friend unsigned_integer<T> operator+( const long long& lhs, unsigned_integer<T> rhs ) { return rhs += lhs; }
		friend unsigned_integer<T> operator-( unsigned_integer<T> lhs, const long long& rhs ) { return lhs -= rhs; }
		friend unsigned_integer<T> operator-( const long long& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) -= rhs; }
		friend unsigned_integer<T> operator/( unsigned_integer<T> lhs, const long long& rhs ) { return lhs /= rhs; }
		friend unsigned_integer<T> operator/( const long long& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) /= rhs; }
		friend unsigned_integer<T> operator%( unsigned_integer<T> lhs, const long long& rhs ) { return lhs %= rhs; }
		friend unsigned_integer<T> operator%( const long long& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) %= rhs; }
		friend unsigned_integer<T> operator^( unsigned_integer<T> lhs, const long long& rhs ) { return lhs ^= rhs; }
		friend unsigned_integer<T> operator^( const long long& lhs, unsigned_integer<T> rhs ) { return rhs ^= lhs; }
		friend unsigned_integer<T> operator&( unsigned_integer<T> lhs, const long long& rhs ) { return lhs &= rhs; }
		friend unsigned_integer<T> operator&( const long long& lhs, unsigned_integer<T> rhs ) { return rhs &= lhs; }
		friend unsigned_integer<T> operator|( unsigned_integer<T> lhs, const long long& rhs ) { return lhs |= rhs; }
		friend unsigned_integer<T> operator|( const long long& lhs, unsigned_integer<T> rhs ) { return rhs |= lhs; };
		friend bool                operator<=( const unsigned_integer<T>& x, const unsigned long long& y ) { return !static_cast<bool>( x > y ); }
		friend bool                operator>=( const unsigned_integer<T>& x, const unsigned long long& y ) { return !static_cast<bool>( x < y ); }
		friend bool                operator>( const unsigned long long& x, const unsigned_integer<T>& y ) { return y < x; }
		friend bool                operator<( const unsigned long long& x, const unsigned_integer<T>& y ) { return y > x; }
		friend bool                operator<=( const unsigned long long& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y < x ); }
		friend bool                operator>=( const unsigned long long& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y > x ); }
		friend bool                operator==( const unsigned long long& y, const unsigned_integer<T>& x ) { return x == y; }
		friend bool                operator!=( const unsigned long long& y, const unsigned_integer<T>& x ) { return !static_cast<bool>( x == y ); }
		friend bool                operator!=( const unsigned_integer<T>& y, const unsigned long long& x ) { return !static_cast<bool>( y == x ); }
		friend unsigned_integer<T> operator*( unsigned_integer<T> lhs, const unsigned long long& rhs ) { return lhs *= rhs; }
		friend unsigned_integer<T> operator*( const unsigned long long& lhs, unsigned_integer<T> rhs ) { return rhs *= lhs; }
		friend unsigned_integer<T> operator+( unsigned_integer<T> lhs, const unsigned long long& rhs ) { return lhs += rhs; }
		friend unsigned_integer<T> operator+( const unsigned long long& lhs, unsigned_integer<T> rhs ) { return rhs += lhs; }
		friend unsigned_integer<T> operator-( unsigned_integer<T> lhs, const unsigned long long& rhs ) { return lhs -= rhs; }
		friend unsigned_integer<T> operator-( const unsigned long long& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) -= rhs; }
		friend unsigned_integer<T> operator/( unsigned_integer<T> lhs, const unsigned long long& rhs ) { return lhs /= rhs; }
		friend unsigned_integer<T> operator/( const unsigned long long& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) /= rhs; }
		friend unsigned_integer<T> operator%( unsigned_integer<T> lhs, const unsigned long long& rhs ) { return lhs %= rhs; }
		friend unsigned_integer<T> operator%( const unsigned long long& lhs, const unsigned_integer<T>& rhs ) { return unsigned_integer<T>( lhs ) %= rhs; }
		friend unsigned_integer<T> operator^( unsigned_integer<T> lhs, const unsigned long long& rhs ) { return lhs ^= rhs; }
		friend unsigned_integer<T> operator^( const unsigned long long& lhs, unsigned_integer<T> rhs ) { return rhs ^= lhs; }
		friend unsigned_integer<T> operator&( unsigned_integer<T> lhs, const unsigned long long& rhs ) { return lhs &= rhs; }
		friend unsigned_integer<T> operator&( const unsigned long long& lhs, unsigned_integer<T> rhs ) { return rhs &= lhs; }
		friend unsigned_integer<T> operator|( unsigned_integer<T> lhs, const unsigned long long& rhs ) { return lhs |= rhs; }
		friend unsigned_integer<T> operator|( const unsigned long long& lhs, unsigned_integer<T> rhs ) { return rhs |= lhs; };
		friend bool                operator>( const unsigned_integer<T>& x, const unsigned_integer<T>& y ) { return y < x; }
		friend bool                operator<=( const unsigned_integer<T>& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( y < x ); }
		friend bool                operator>=( const unsigned_integer<T>& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( x < y ); }
		friend bool                operator!=( const unsigned_integer<T>& x, const unsigned_integer<T>& y ) { return !static_cast<bool>( x == y ); }
		friend unsigned_integer<T> operator*( unsigned_integer<T> lhs, const unsigned_integer<T>& rhs ) { return lhs *= rhs; }
		friend unsigned_integer<T> operator+( unsigned_integer<T> lhs, const unsigned_integer<T>& rhs ) { return lhs += rhs; }
		friend unsigned_integer<T> operator-( unsigned_integer<T> lhs, const unsigned_integer<T>& rhs ) { return lhs -= rhs; }
		friend unsigned_integer<T> operator/( unsigned_integer<T> lhs, const unsigned_integer<T>& rhs ) { return lhs /= rhs; }
		friend unsigned_integer<T> operator%( unsigned_integer<T> lhs, const unsigned_integer<T>& rhs ) { return lhs %= rhs; }
		friend unsigned_integer<T> operator^( unsigned_integer<T> lhs, const unsigned_integer<T>& rhs ) { return lhs ^= rhs; }
		friend unsigned_integer<T> operator&( unsigned_integer<T> lhs, const unsigned_integer<T>& rhs ) { return lhs &= rhs; }
		friend unsigned_integer<T> operator|( unsigned_integer<T> lhs, const unsigned_integer<T>& rhs ) { return lhs |= rhs; };
		friend unsigned_integer<T> operator++( unsigned_integer<T>& x, int )
		{
			unsigned_integer<T> nrv( x );
			++x;
			return nrv;
		};
		friend unsigned_integer<T> operator--( unsigned_integer<T>& x, int )
		{
			unsigned_integer<T> nrv( x );
			--x;
			return nrv;
		};

	private:
		unsigned_integer<T> operator-() const
		{
			return unsigned_integer<T>( invalid );
		}
		T m_value;
	};
} // namespace stk

namespace std {
	template <typename T>
	stk::unsigned_integer<T> sqrt( const stk::unsigned_integer<T>& v )
	{
		T vd = (T)v;
		vd = std::sqrt( vd );
		return stk::unsigned_integer<T>( vd );
	}
	template <typename T>
	stk::unsigned_integer<T> cos( const stk::unsigned_integer<T>& v )
	{
		T vd = (T)v;
		vd = std::cos( vd );
		return stk::unsigned_integer<T>( vd );
	}
	template <typename T>
	stk::unsigned_integer<T> sin( const stk::unsigned_integer<T>& v )
	{
		T vd = (T)v;
		vd = std::sin( vd );
		return stk::unsigned_integer<T>( vd );
	}
	template <typename T>
	stk::unsigned_integer<T> tan( const stk::unsigned_integer<T>& v )
	{
		T vd = (T)v;
		vd = std::tan( vd );
		return stk::unsigned_integer<T>( vd );
	}
	template <typename T>
	stk::unsigned_integer<T> atan( const stk::unsigned_integer<T>& v )
	{
		T vd = (T)v;
		vd = std::atan( vd );
		return stk::unsigned_integer<T>( vd );
	}
	template <typename T>
	stk::unsigned_integer<T> acos( const stk::unsigned_integer<T>& v )
	{
		T vd = (T)v;
		vd = std::acos( vd );
		return stk::unsigned_integer<T>( vd );
	}
	template <typename T>
	stk::unsigned_integer<T> asin( const stk::unsigned_integer<T>& v )
	{
		T vd = (T)v;
		vd = std::asin( vd );
		return stk::unsigned_integer<T>( vd );
	}
	template <typename T>
	stk::unsigned_integer<T> exp( const stk::unsigned_integer<T>& v )
	{
		T vd = (T)v;
		vd = std::exp( vd );
		return stk::unsigned_integer<T>( vd );
	}
	template <typename T>
	stk::unsigned_integer<T> log10( const stk::unsigned_integer<T>& v )
	{
		T vd = (T)v;
		vd = std::log10( vd );
		return stk::unsigned_integer<T>( vd );
	}
	template <typename T>
	stk::unsigned_integer<T> log( const stk::unsigned_integer<T>& v )
	{
		T vd = (T)v;
		vd = std::log( vd );
		return stk::unsigned_integer<T>( vd );
	}
	template <typename T>
	stk::unsigned_integer<T> ceil( const stk::unsigned_integer<T>& v )
	{
		T vd = (T)v;
		vd = std::ceil( vd );
		return stk::unsigned_integer<T>( vd );
	}
	template <typename T>
	stk::unsigned_integer<T> floor( const stk::unsigned_integer<T>& v )
	{
		T vd = (T)v;
		vd = std::floor( vd );
		return stk::unsigned_integer<T>( vd );
	}
	template <typename T>
	class numeric_limits<stk::unsigned_integer<T>> : public numeric_limits<T>
	{
	public:
		static stk::unsigned_integer<T>( min )()
		{
			return stk::unsigned_integer<T>( ( std::numeric_limits<T>::min )() );
		}
		static stk::unsigned_integer<T>( max )()
		{
			return stk::unsigned_integer<T>( ( std::numeric_limits<T>::max )() - 1 );
		}
	};
} // namespace std
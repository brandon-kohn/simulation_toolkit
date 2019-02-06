#pragma once

#include <junction/ConcurrentMap_Leapfrog.h>
#include <junction/Core.h>
#include <turf/Util.h>
#include <geometrix/utility/assert.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/optional.hpp>
#include <cstdint>
#include <memory>

namespace turf { namespace util {
	template <>
	struct BestFit<float>
	{
		using Unsigned = std::uint32_t;
		using Signed = std::int32_t;
	};
	template <>
	struct BestFit<double>
	{
		using Unsigned = std::uint64_t;
		using Signed = std::int64_t;
	};
	template <>
	struct BestFit<long double>
	{
		using Unsigned = std::uintmax_t;
		using Signed = std::intmax_t;
	};
}}//! namespace turf::util;

namespace stk {
	namespace unordered_map_detail {

		template <typename T, typename EnableIf=void>
		struct data_value_traits
		{
			using Value = T;
			using IntType = typename turf::util::BestFit<T>::Unsigned;
			static const IntType NullValue = (std::numeric_limits<T>::max)();
			static const IntType Redirect = (std::numeric_limits<T>::max)() - 1;
		};
		
		template <typename T>
		struct data_value_traits<T, typename std::enable_if<std::is_floating_point<T>::value>::type>
		{
			using Value = T;
			using IntType = typename turf::util::BestFit<T>::Unsigned;
			static const IntType NullValue = (std::numeric_limits<T>::max)();
			static const IntType Redirect = std::numeric_limits<T>::lowest();
		};
		
		template <typename T>
		struct data_value_traits<T, typename std::enable_if<std::is_pointer<T>::value>::type>
		{
			using Value = T * ;
			using IntType = typename turf::util::BestFit<T*>::Unsigned;
			static const IntType NullValue = 0;
			static const IntType Redirect = 1;
		};

	}//! namespace unordered_map_detail;

	template<typename Key_, typename Data, typename MemoryReclamationPolicy = junction::DefaultMemoryReclamationPolicy>
	class concurrent_numeric_unordered_map
	{
		static_assert(std::is_integral<Data>::value || std::is_floating_point<Data>::value || std::is_pointer<Data>::value, "The Data type must be a fundamental arithmetic type or a pointer.");
		static_assert((std::is_integral<Key_>::value || std::is_pointer<Key_>::value) && sizeof(Key_) <= sizeof(std::uint64_t), "The Key type of concurrent_unordered_map must be convertible to std::uint64_t. i.e. either an integral type or a pointer.");

		struct uint64_key_traits
		{
			using Key = std::uint64_t;
			using Hash = turf::util::BestFit<Key>::Unsigned;
			static const Key NullKey = UINT64_MAX;
			static const Hash NullHash = Hash(NullKey);
			static Hash hash(Key key)
			{
				return turf::util::avalanche(Hash(key));
			}
			static Key dehash(Hash hash)
			{
				return (Key)turf::util::deavalanche(hash);
			}
		};

	public:

		using key_type = Key_;
		using mapped_type = Data;

	private:
		using map_type = junction::ConcurrentMap_Leapfrog<std::uint64_t, mapped_type, uint64_key_traits, stk::unordered_map_detail::data_value_traits<Data>, MemoryReclamationPolicy>;

		template <typename T>
		class hash_iterator : public boost::iterator_facade<hash_iterator<T>, T, boost::forward_traversal_tag>
		{
		public:

			hash_iterator() = default;

			explicit hash_iterator(typename map_type::Iterator const& it)
				: m_iterator(it)
			{}

			template <typename U>
			hash_iterator(hash_iterator<U> const& o)
				: m_iterator(o.m_iterator)
			{}

			~hash_iterator()
			{}

			bool is_valid() const
			{
				return m_iterator.isValid() && m_iterator->getValue() != typename map_type::ValueTraits::NullValue;
			}

			key_type key() const
			{
				return reinterpret_cast<key_type>(m_iterator.getKey());
			}
			T       value() const
			{
				return dereference();
			}

		private:

			friend class boost::iterator_core_access;
			template <typename> friend class hash_iterator;

			void increment()
			{
				if (m_iterator.isValid())
					m_iterator.next();
			}

			template <typename U>
			bool equal(hash_iterator<U> const& other) const
			{
				return m_iterator == other.m_iterator;
			}

			T dereference() const
			{
				return m_iterator.getValue();
			}

			typename map_type::Iterator m_iterator;
		};

	public:

		using value_type = mapped_type;
		using iterator = hash_iterator<value_type>;
		using const_iterator = hash_iterator<const value_type>;

		concurrent_numeric_unordered_map() = default;

		~concurrent_numeric_unordered_map()
		{
			clear();
		}

		boost::optional<Data> find(key_type k) const
		{
			auto iter = m_map.find((std::uint64_t)k);
			if (iter.isValid()) {
				GEOMETRIX_ASSERT(iter.getValue() != (mapped_type)stk::unordered_map_detail::data_value_traits<Data>::Redirect);
				return iter.getValue();
			}

			return boost::none;
		}

		std::pair<Data, bool> insert(key_type k, value_type data)
		{
			auto mutator = m_map.insertOrFind((std::uint64_t)k);
			auto result = mutator.getValue();
			auto wasInserted = false;
			if (result == (mapped_type)stk::unordered_map_detail::data_value_traits<Data>::NullValue)
			{
				result = data;
				auto oldData = mutator.exchangeValue(result);

				//! If a new value has been put into the key which isn't result.. get it.
				wasInserted = oldData != result;
				if (!wasInserted)
					result = mutator.getValue();
			}

			return std::make_pair(result, wasInserted);
		}

		void erase(key_type k)
		{
			auto iter = m_map.find((std::uint64_t)k);
			if (iter.isValid())
				iter.eraseValue();
		}

		template <typename Fn>
		void for_each(Fn&& fn)
		{
			auto it = typename map_type::Iterator(m_map);
			while (it.isValid()) 
			{
				auto k = it.getKey();
				auto value = it.getValue();
				fn(k, value);
				it.next();
			}
		}

		//! Clear is not thread safe.
		void clear()
		{
			auto it = typename map_type::Iterator(m_map);
			while (it.isValid()) 
			{
				m_map.erase(it.getKey());
				it.next();
			}
		}

		//! Iterator interface
		bool empty() const
		{
			return cbegin() == cend();
		}
		iterator begin()
		{
			return iterator{ typename map_type::Iterator{ m_map } };
		}
		iterator end()
		{
			return iterator{ typename map_type::Iterator{} };
		}
		const_iterator begin() const
		{
			return const_iterator{ typename map_type::Iterator{ m_map } };
		}
		const_iterator end() const
		{
			return const_iterator{ typename map_type::Iterator{} };
		}
		const_iterator cbegin() const
		{
			return const_iterator{ typename map_type::Iterator{ m_map } };
		}
		const_iterator cend() const
		{
			return const_iterator{ typename map_type::Iterator{} };
		}

		void quiesce()
		{
			m_map.getMemoryReclaimer().quiesce();
		}

	private:

		mutable map_type m_map;

	};

}//! namespace stk;

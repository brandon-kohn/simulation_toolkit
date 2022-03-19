#pragma once

#include <junction/ConcurrentMap_Leapfrog.h>
#include <junction/Core.h>
#include <turf/Util.h>
#include <geometrix/utility/assert.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <cstdint>
#include <memory>

namespace stk { namespace unordered_map_detail {

		template <typename T, typename EnableIf=void>
		struct uint64_key_traits;

		template <typename T>
		struct uint64_key_traits<T, typename std::enable_if<std::is_integral<T>::value>::type>
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

		template <typename T>
		struct uint64_key_traits<T, typename std::enable_if<std::is_pointer<T>::value>::type>
		{
			using Key = std::uint64_t;
			using Hash = turf::util::BestFit<Key>::Unsigned;
			static const Key NullKey = Key(0);
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

        template <typename T>
        struct pointer_value_traits
        {
            using Value = T*;
            using IntType = typename turf::util::BestFit<T*>::Unsigned;
            static const IntType NullValue = 0;
            static const IntType Redirect = 1;
			static bool is_valid(Value v)
			{
				return NullValue != (IntType)v && Redirect != (IntType)v;
			}

        };

    }//! namespace unordered_map_detail;

	//! A concurrent map that holds pointers to data and exercises ownership semantics over those pointers.
	//! NOTE: Does not support holding null ptrs as data.
	//! NOTE: If a null_deleter policy is used, the emplace method will leak data if not externally managed.
    template<typename Key_, typename Data, typename OnErasePolicy = std::default_delete<Data>, typename MemoryReclamationPolicy = junction::DefaultMemoryReclamationPolicy>
    class concurrent_pointer_unordered_map
    {
        static_assert(std::is_default_constructible<OnErasePolicy>::value, "The OnErasePolicy of concurrent_unordered_map must be default constructible and ideally stateless.");
		static_assert((std::is_integral<Key_>::value || std::is_pointer<Key_>::value) && sizeof(Key_) <= sizeof(std::uint64_t), "The Key type of concurrent_pointer_unordered_map must be convertible to std::uint64_t. i.e. either an integral type or a pointer.");

		using key_traits = stk::unordered_map_detail::uint64_key_traits<Key_>;
		using pointer_traits = stk::unordered_map_detail::pointer_value_traits<Data>;

	public:

        using erase_policy = OnErasePolicy;
        using key_type = Key_;
        using data_ptr = Data*;

	private:
        using map_type = junction::ConcurrentMap_Leapfrog<std::uint64_t, data_ptr, key_traits, pointer_traits, MemoryReclamationPolicy>;

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
				return m_iterator.isValid() && m_iterator->getValue() != pointer_traits::NullValue;
			}

            key_type key() const { return (key_type)m_iterator.getKey(); }
            T&       value() const { return dereference(); }

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

            T& dereference() const
            {
				auto pData = m_iterator.getValue();
				GEOMETRIX_ASSERT(pData);
				return *pData;
            }

			typename map_type::Iterator m_iterator;
        };

	public:

		using value_type = Data;
		using iterator = hash_iterator<value_type>;
		using const_iterator = hash_iterator<const value_type>;

        concurrent_pointer_unordered_map() = default;

		concurrent_pointer_unordered_map(const MemoryReclamationPolicy& mp)
			: m_map(mp)
		{}

        ~concurrent_pointer_unordered_map()
        {
            clear();
        }

        data_ptr find(key_type k) const
        {
            auto iter = m_map.find((std::uint64_t)k);
            if(iter.isValid())
            {
                GEOMETRIX_ASSERT(iter.getValue() != (data_ptr)pointer_traits::Redirect);
                return iter.getValue();
            }

            return nullptr;
        }

        std::pair<data_ptr, bool> insert(key_type k, std::unique_ptr<Data, erase_policy>&& pData)
        {
			GEOMETRIX_ASSERT(pointer_traits::is_valid(pData.get()));
			auto mutator = m_map.insertOrFind((std::uint64_t)k);
            auto result = mutator.getValue();
            auto wasInserted = false;
            if (result == nullptr)
            {
                result = pData.release();
                auto oldData = mutator.exchangeValue(result);
                if (oldData)
                    m_map.getMemoryReclaimer().template reclaim_via_defaultable_callable<erase_policy>(oldData);

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
            {
                auto pValue = iter.eraseValue();
                if (pValue != (data_ptr)pointer_traits::NullValue)
                    m_map.getMemoryReclaimer().template reclaim_via_defaultable_callable<erase_policy>(pValue);
            }
        }
       
        //! Bypass the memory reclaimer and invoke the erase policy directly. Use if thread-safe reclamation.
        void erase_direct(key_type k)
        {
            auto iter = m_map.find((std::uint64_t)k);
            if (iter.isValid())
            {
                auto pValue = iter.eraseValue();
                if (pValue != (data_ptr)pointer_traits::NullValue)
                    erase_policy()(pValue);
            }
        }

        template <typename Fn>
        void for_each(Fn&& fn)
        {
            auto it = typename map_type::Iterator(m_map);
            while(it.isValid())
            {
                auto k = it.getKey();
                auto pValue = it.getValue();
                fn(k, pValue);
                it.next();
            }
        }

        //! Clear is not thread safe.
        void clear()
        {
            auto it = typename map_type::Iterator(m_map);
            while(it.isValid())
            {
                auto pValue = it.getValue();
                erase_policy()(pValue);
                m_map.erase(it.getKey());
                it.next();
            };
        }

		//! Iterator interface - iteration is not safe in the presence of inserts.
		bool empty() const { return cbegin() == cend(); }
		iterator begin() { return iterator{ typename map_type::Iterator{ m_map } }; }
		iterator end() { return iterator{ typename map_type::Iterator{} }; }
		const_iterator begin() const { return const_iterator{ typename map_type::Iterator{ m_map } }; }
		const_iterator end() const { return const_iterator{ typename map_type::Iterator{} }; }
		const_iterator cbegin() const { return const_iterator{ typename map_type::Iterator{ m_map } }; }
		const_iterator cend() const { return const_iterator{ typename map_type::Iterator{} }; }

        void quiesce()
        {
            m_map.getMemoryReclaimer().quiesce();
        }

    private:

        mutable map_type m_map;

    };

}//! namespace stk;

#pragma once

#include <junction/ConcurrentMap_Leapfrog.h>
#include <junction/Core.h>
#include <turf/Util.h>
#include <geometrix/utility/assert.hpp>
#include <cstdint>
#include <memory>

namespace stk { namespace detail {
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

        template <typename T>
        struct pointer_value_traits
        {
            using Value = T*;
            using IntType = typename turf::util::BestFit<T*>::Unsigned;
            static const IntType NullValue = 0;
            static const IntType Redirect = 1;
        };

    }//! namespace detail;

	//! The erase policy specifies ownership semantics of the pointers to Data held by the map.
	template<typename Data, typename OnErasePolicy = std::default_delete<Data>>
    class concurrent_integral_map
    {
    public:

		static_assert(std::is_default_constructible<OnErasePolicy>::value, "The OnErasePolicy of concurrent_integral_map must be default constructible and ideally stateless.");

		using erase_policy = OnErasePolicy;
        using data_ptr = Data*;
        using map_type = junction::ConcurrentMap_Leapfrog<std::uint64_t, data_ptr, stk::detail::uint64_key_traits, stk::detail::pointer_value_traits<Data>>;

		concurrent_integral_map() = default;

        ~concurrent_integral_map()
        {
            clear();
        }

        data_ptr find(std::uint64_t k) const
        {
            auto iter = m_map.find(k);
			if(iter.isValid())
			{
				GEOMETRIX_ASSERT(iter.getValue() != (data_ptr)stk::detail::pointer_value_traits<Data>::Redirect);
				return iter.getValue();
			}

			return nullptr;
        }

        template <typename Deleter>
        std::pair<data_ptr, bool> insert(std::uint64_t k, std::unique_ptr<Data, Deleter>&& pData)
        {
            auto mutator = m_map.insertOrFind(k);
            auto result = mutator.getValue();
			bool wasInserted = false;
			if (result == nullptr)
			{
				result = pData.release();
				auto oldData = mutator.exchangeValue(result);
				if (oldData)
					junction::DefaultQSBR().enqueue<erase_policy>(oldData);

				//! If a new value has been put into the key which isn't result.. get it.
				wasInserted = oldData != result;
				if (!wasInserted)
					result = mutator.getValue();
			}

            return std::make_pair(result, wasInserted);
        }

		template <typename... Args>
		std::pair<data_ptr, bool> emplace(std::uint64_t k, Args&&... a) 
		{
			auto mutator = m_map.insertOrFind(k);
			auto result = mutator.getValue();
			bool wasInserted = false;
			if (result == nullptr)
			{
				result = new Data(a...);
				auto oldData = mutator.exchangeValue(result);
				if (oldData)
					junction::DefaultQSBR().enqueue<erase_policy>(oldData);

				//! If a new value has been put into the key which isn't result.. get it.
				wasInserted = oldData != result;
				if (!wasInserted)
					result = mutator.getValue();
			}

			return std::make_pair(result, wasInserted);
		}

		void erase(std::uint64_t k)
        {
			auto iter = m_map.find(k);
            if (iter.isValid())
            {
				auto pValue = iter.eraseValue();
				if (pValue != (data_ptr)stk::detail::pointer_value_traits<Data>::NullValue)
					junction::DefaultQSBR().enqueue<erase_policy>(pValue);
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

		static void quiesce()
		{
			junction::DefaultQSBR().flush();
		}

    private:

        mutable map_type m_map;

    };

}//! namespace stk;

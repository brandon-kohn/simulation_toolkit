#pragma once

#include <junction/ConcurrentMap_Leapfrog.h>
#include <junction/Core.h>
#include <turf/Util.h>
#include <cstdint>

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

    template<typename Data>
    class concurrent_integral_map
    {
    public:

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
            GEOMETRIX_ASSERT(iter.getValue() != (data_ptr)stk::pointer_value_traits<Data>::Redirect);
            return iter.getValue();
        }

        std::pair<data_ptr, bool> insert(std::uint64_t k, data_ptr pData)
        {
            auto mutator = m_map.insertOrFind(k);
            auto result = mutator.getValue();
            if (result == nullptr)
            {
                if (result != mutator.exchangeValue(pData))
                    result = mutator.getValue();
            }

            GEOMETRIX_ASSERT(result != nullptr);
            return std::make_pair(result, result == pData);
        }

		void erase(std::uint64_t k)
        {
			auto iter = m_map.find(k);
            if (iter.isValid())
            {
                delete pValue;
                iter.eraseValue();
            }
        }

        //! Clear is not thread safe.
        void clear()
        {
            auto it = typename map_type::Iterator(m_map);
            while(it.isValid())
            {
                auto pValue = it.getValue();
                delete pValue;
                m_map.erase(it.getKey());
                it.next();
            };
        }

    private:

        map_type m_map;

    };

}//! namespace stk;

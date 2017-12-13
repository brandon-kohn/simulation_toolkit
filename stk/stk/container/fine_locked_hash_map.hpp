//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_CONTAINER_FINE_LOCKED_HASH_MAP_HPP
#define STK_CONTAINER_FINE_LOCKED_HASH_MAP_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include <stk/container/locked_list.hpp>
#include <boost/optional.hpp>
#include <functional>

namespace stk {

    //! A hash table which stores a shared_mutex per node in the list. Very memory intensive for many elements.
    template <typename Key, typename Value, typename Hash = std::hash<Key>, typename Mutex = std::mutex>
    class fine_locked_hash_map
    {
        class bucket_type
        {
            using bucket_value = std::pair<Key, Value>;
            using bucket_data = locked_list<bucket_value, Mutex>;
            
            bucket_data data;

            std::shared_ptr<bucket_value> find_entry_for(Key const& key) const
            {
                return data.find_first_if([&](bucket_value const& item) { return item.first == key; });
            }

        public:

            boost::optional<Value> find(Key const& key) const
            {
                auto it = data.find([&key](bucket_value const& item) { return item.first == key; });
				return it ? boost::optional<Value>(it->second) : boost::optional<Value>();
            }

			bool add(Key const& key, Value const& value)
            {
                return data.add_back(bucket_value{ key, value }, [&](bucket_value const& item){ return item.first == key; });
            }

            void add_or_update(Key const& key, Value const& value)
            {
                data.update_or_add_back(bucket_value{ key, value }, [&](bucket_value const& item){ return item.first == key; });
            }

            void remove(Key const& key)
            {
                data.remove_if([&](const bucket_value& item) { return key == item.first; });
            }
        };

        std::vector<std::unique_ptr<bucket_type>> buckets;
        Hash hasher;

        bucket_type& get_bucket(Key const& key) const
        {
            const auto bucket_index = hasher(key) % buckets.size();
            return *buckets[bucket_index];
        }

    public:

        using key_type = Key;
        using mapped_type = Value;
        using hash_type = Hash;

        fine_locked_hash_map(unsigned num_buckets = 1024, Hash const& hasher = Hash())
            : buckets(num_buckets)
            , hasher(hasher)
        {
            for (unsigned i = 0; i < num_buckets; ++i)
                buckets[i].reset(new bucket_type);
        }
		
        fine_locked_hash_map(const fine_locked_hash_map&) = delete;
        fine_locked_hash_map& operator=(const fine_locked_hash_map&) = delete;

        boost::optional<mapped_type> find(Key const& key) const
        {
            return get_bucket(key).find(key);
        }

		bool add(Key const& key, Value const& value)
        {
            return get_bucket(key).add(key, value);
        }

        void add_or_update(Key const& key, Value const& value)
        {
            get_bucket(key).add_or_update(key, value);
        }

        void remove(Key const& key)
        {
            get_bucket(key).remove(key);
        }
    };

}//! namespace stk;

#endif//! STK_CONTAINER_FINE_LOCKED_HASH_MAP_HPP
 
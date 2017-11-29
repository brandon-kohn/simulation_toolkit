//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_CONTAINER_LOCKED_HASH_MAP_HPP
#define STK_CONTAINER_LOCKED_HASH_MAP_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include <boost/range/algorithm/find_if.hpp>

template <typename Key, typename Value, typename Hash=std::hash<Key>>
class locked_hash_map
{
    class bucket_type
    {
        using bucket_value = std::pair<Key,Value>;
        using bucket_data = std::list<buket_value>;
        using bucket_iterator = typename bucket_data::iterator;
      
        bucket_data data;
        mutable boost::shared_mutex mutex;
      
        bucket_iterator find_entry_for(Key const& key) const
        {
            return boost::find_if(data, [&](bucket_value const& item){ return item.first == key; });
        }
        
    public:
    
        Value value_for(Key const& key, Value const& default_value) const
        {
            auto lock = boost::shared_lock<boost::shared_mutex>{ mutex };
            const auto found_entry = find_entry_for(key);
            return found_entry==data.end() ? default_value : found_entry->second;
        }
        
        void add_or_update_mapping(Key const& key, Value const& value)
        {
            auto lock = std::unique_lock<boost::shared_mutex>{mutex};
            const auto found_entry = find_entry_for(key);
            if( found_entry == data.end())
                data.push_back(bucket_value{key, value});
            else
                found_entry->second = value;
            
        }
        
        void remove_mapping(Key const& key)
        {
            auto lock = std::unique_lock<boost::shared_mutex>{mutex};
            const auto found_entry = find_entry_for(key);
            if( found_entry != data.end())
                data.erase(found_entry);
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
    
    locked_hash_map(unsigned num_buckets=19, Hash const& hasher = Hash())
        : buckets(num_buckets)
        , hasher(hasher)
    {
        for(unsigned i = 0; i < num_buckets; ++i)
            buckets[i].reset(new bucket_type);
    }
    
    locked_hash_map(const locked_hash_map&) = delete;
    locked_hash_map& operator=(const locked_hash_map&) = delete;
    
    mapped_type value_for(Key const& key, Value const& default_value=Value()) const
    {
        return get_bucket(key)value_for(key, default_value);
    }
    
    void add_or_update_mapping(Key const& key, Value const& value)
    {
        get_bucket(key).add_or_update_mapping(key, value);
    }
    
    void remove_mapping(Key const& key)
    {
        get_bucket(key).remove_mapping(key);
    }
};

#endif//! STK_CONTAINER_LOCKED_HASH_MAP_HPP

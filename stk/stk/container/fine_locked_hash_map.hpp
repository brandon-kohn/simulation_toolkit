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
#include <boost/thread/shared_mutex.hpp>
#include <boost/optional.hpp>
#include <functional>

namespace stk {

	//! A hash table which stores a shared_mutex per node in the list. Very memory intensive for many elements.
	template <typename Key, typename Value, typename Hash = std::hash<Key>>
	class fine_locked_hash_map
	{
		class bucket_type
		{
			using bucket_value = std::pair<Key, Value>;
			using bucket_data = locked_list<bucket_value>;
			
			bucket_data data;
			
			std::shared_ptr<bucket_value> find_entry_for(Key const& key) const
			{
				return data.find_first_if([&](bucket_value const& item) { return item.first == key; });
			}

		public:

			Value value_for(Key const& key, Value const& default_value) const
			{
				const auto found_entry = find_entry_for(key);
				return !found_entry ? default_value : found_entry->second;
			}

			boost::optional<Value> find(Key const& key) const
			{
				const auto found_entry = find_entry_for(key);
				return found_entry ? boost::optional<Value>(found_entry->second) : boost::optional<Value>();
			}

			void add_or_update_mapping(Key const& key, Value const& value)
			{
				const auto found_entry = find_entry_for(key);
				if (!found_entry)
					data.push_front(bucket_value{ key, value });
				else
					found_entry->second = value;

			}

			void remove_mapping(Key const& key)
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

		fine_locked_hash_map(unsigned num_buckets = 19, Hash const& hasher = Hash())
			: buckets(num_buckets)
			, hasher(hasher)
		{
			for (unsigned i = 0; i < num_buckets; ++i)
				buckets[i].reset(new bucket_type);
		}

		fine_locked_hash_map(const fine_locked_hash_map&) = delete;
		fine_locked_hash_map& operator=(const fine_locked_hash_map&) = delete;

		mapped_type value_for(Key const& key, Value const& default_value = Value()) const
		{
			return get_bucket(key).value_for(key, default_value);
		}

		boost::optional<mapped_type> find(Key const& key) const
		{
			return get_bucket(key).find(key);
		}

		void add_or_update(Key const& key, Value const& value)
		{
			get_bucket(key).add_or_update_mapping(key, value);
		}

		void remove(Key const& key)
		{
			get_bucket(key).remove_mapping(key);
		}
	};

}//! namespace stk;

#endif//! STK_CONTAINER_FINE_LOCKED_HASH_MAP_HPP

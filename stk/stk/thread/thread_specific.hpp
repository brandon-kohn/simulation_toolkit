//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/thread/thread_local_pod.hpp>
#include <geometrix/utility/assert.hpp>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <type_traits>

namespace stk { namespace thread {

    template <typename Value>
    struct thread_specific_unordered_map_policy
    {
        template <typename Key>
        struct map_type_generator
        {
            using type = std::unordered_map<Key, Value>;
        };

        template <typename Map>
        static void initialize(Map& m)
        {}

        template <typename Map, typename Key>
        static Value* find(Map& m, const Key& k)
        {
            auto it = m.find(k);
            return it != m.end() ? &it->second : nullptr;
        }

        template <typename Map, typename Key>
        static Value* insert(Map& m, const Key& k, Value&& v)
        {
            return &m.insert(std::make_pair(k, std::forward<Value>(v))).first->second;
        }

        template <typename Map, typename Key>
        static void erase(Map& m, const Key& k)
        {
            m.erase(k);
        }

        template <typename Map>
        static bool empty(Map& m)
        {
            return m.empty();
        }

        template <typename Map, typename Visitor>
        static void for_each(Map& m, Visitor&& visitor)
        {
            for (auto& i : m)
                visitor(i.first, i.second);
        }
    };

    template <typename Value>
    struct thread_specific_std_map_policy
    {
        template <typename Key>
        struct map_type_generator
        {
            using type = std::map<Key, Value>;
        };

        template <typename Map>
        static void initialize(Map& m)
        {}

        template <typename Map, typename Key>
        static Value* find(Map& m, const Key& k)
        {
            auto it = m.find(k);
            return it != m.end() ? &it->second : nullptr;
        }

        template <typename Map, typename Key>
        static Value* insert(Map& m, const Key& k, Value&& v)
        {
            return &m.insert(std::make_pair(k, std::forward<Value>(v))).first->second;
        }

        template <typename Map, typename Key>
        static void erase(Map& m, const Key& k)
        {
            m.erase(k);
        }

        template <typename Map>
        static bool empty(Map& m)
        {
            return m.empty();
        }

        template <typename Map, typename Visitor>
        static void for_each(Map& m, Visitor&& visitor)
        {
            for (auto& i : m)
                visitor(i.first, i.second);
        }
    };

	template <typename Value>
	struct thread_specific_flat_map_policy
	{
		template <typename Key>
		struct map_type_generator
		{
			using type = boost::container::flat_map<Key, std::unique_ptr<Value>>;
		};

		template <typename Map>
		static void initialize(Map& m)
		{
			m.reserve(100);
		}

		template <typename Map, typename Key>
		static Value* find(Map& m, const Key& k)
		{
			auto it = m.find(k);
			return it != m.end() ? it->second.get() : nullptr;
		}

		template <typename Map, typename Key>
		static Value* insert(Map& m, const Key& k, Value&& v)
		{
			return m.insert(std::make_pair(k, boost::make_unique<Value>(std::forward<Value>(v)))).first->second.get();
		}

		template <typename Map, typename Key>
		static void erase(Map& m, const Key& k)
		{
			m.erase(k);
		}

		template <typename Map>
		static bool empty(Map& m)
		{
			return m.empty();
		}

		template <typename Map, typename Visitor>
		static void for_each(Map& m, Visitor&& visitor)
		{
			for (auto& i : m)
				visitor(i.first, *i.second);
		}
	};

	template <typename Value, std::size_t S = 100>
	struct thread_specific_fixed_flat_map_policy
	{
		template <typename Key>
		struct map_type_generator
		{
			using type = boost::container::flat_map<Key, Value>;
		};

		template <typename Map>
		static void initialize(Map& m)
		{
			m.reserve(S);
		}

		template <typename Map, typename Key>
		static Value* find(Map& m, const Key& k)
		{
			auto it = m.find(k);
			return it != m.end() ? &it->second : nullptr;
		}

		template <typename Map, typename Key>
		static Value* insert(Map& m, const Key& k, Value&& v)
		{
			GEOMETRIX_ASSERT(m.size() < S);
			return &m.insert(std::make_pair(k, std::forward<Value>(v))).first->second;
		}

		template <typename Map, typename Key>
		static void erase(Map& m, const Key& k)
		{
			m.erase(k);
		}

		template <typename Map>
		static bool empty(Map& m)
		{
			return m.empty();
		}

		template <typename Map, typename Visitor>
		static void for_each(Map& m, Visitor&& visitor)
		{
			for (auto& i : m)
				visitor(i.first, i.second);
		}
	};

	struct default_thread_specific_tag{};

    //! Creates a thread local object which can be scoped to an objects lifetime.
	//! NOTE: thread_specific instances should either outlive the threads who access them, or should go out of scope when not being accessed by any threads.
	//! Violations of either condition are undefined behavior.
    template <typename T, typename MapPolicy = thread_specific_std_map_policy<T>, typename Tag = default_thread_specific_tag>
    class thread_specific
    {
        using data_ptr = T*;
        using const_data_ptr = typename std::add_const<data_ptr>::type;
        using map_policy = MapPolicy;
        struct instance_map : map_policy::template map_type_generator<thread_specific<T, MapPolicy, Tag> const*>::type
        {
            instance_map()
            {
                map_policy::initialize(*this);
            }

            instance_map(const instance_map&)=delete;
            instance_map& operator=(const instance_map&)=delete;
            instance_map(instance_map&&)=delete;
            instance_map& operator=(instance_map&&)=delete;
            ~instance_map()
            {
                map_policy::for_each(*this, [this](thread_specific<T, map_policy, Tag> const* key, T& v)
                {
                    if (key->m_deinitializer)
                        key->m_deinitializer(v);
                    key->remove_instance_map(this);
                });
            }
        };
        friend struct instance_map;
        static instance_map& hive();

    public:

        using reference = typename std::add_lvalue_reference<T>::type;
        using const_reference = typename std::add_const<reference>::type;

        thread_specific()
            : m_initializer([](){ return T();})
        {}

        ~thread_specific()
        {
#ifndef NDEBUG
            m_isBeingDestructed = true;
#endif
            destroy();
        }

        thread_specific(const thread_specific&) = delete;
        thread_specific& operator=(const thread_specific&) = delete;
        thread_specific(thread_specific&&) = delete;
        thread_specific& operator=(thread_specific&&) = delete;

        template <typename Initializer>
        thread_specific( Initializer&& init )
            : m_initializer( std::forward<Initializer>(init) )
        {}

        template <typename Initializer, typename Deinitializer>
        thread_specific( Initializer&& init, Deinitializer&& deinit)
            : m_initializer( std::forward<Initializer>(init) )
            , m_deinitializer( std::forward<Deinitializer>(deinit) )
        {}

        template <typename Value>
        thread_specific& operator = ( Value&& t )
        {
            auto pData = get_or_add_item();
            GEOMETRIX_ASSERT(pData);
            *pData = std::forward<Value>(t);
            return *this;
        }

        reference operator *()
        {
            return *get_or_add_item();
        }

        const_reference operator *() const
        {
            return *get_or_add_item();
        }

        T* operator ->()
        {
            return get_or_add_item();
        }

        T const* operator ->() const
        {
            return get_or_add_item();
        }

        bool has_value_on_calling_thread() const
        {
            return get_item() != nullptr;
        }

        explicit operator bool() const
        {
            return has_value_on_calling_thread();
        }

        template <typename Fn>
        void for_each_thread_value(Fn&& fn) const
        {
            auto lk = std::unique_lock<std::mutex> { m_mutex };
            for(auto pMap : m_maps)
            {
                auto r = map_policy::find(*pMap, this);
                if(r)
                    fn(*r);
            }
        }

    private:

        data_ptr get_item() const
        {
            GEOMETRIX_ASSERT(!m_isBeingDestructed);
            auto& m = hive();
            return map_policy::find(m,this);
        }

        data_ptr get_or_add_item () const
        {
            GEOMETRIX_ASSERT(!m_isBeingDestructed);
            auto& m = hive();

            auto r = map_policy::find(m,this);
            if (r)
                return r;

            {
                auto lk = std::unique_lock<std::mutex> { m_mutex };
                m_maps.insert(&m);
            }

            return map_policy::insert(m, this, m_initializer());
        }

        void remove_instance_map(instance_map* pMap) const
        {
            GEOMETRIX_ASSERT(!m_isBeingDestructed);
            auto lk = std::unique_lock<std::mutex> { m_mutex };
            m_maps.erase(pMap);
        }

        void destroy()
        {
            auto& m = hive();
            auto key = this;
            if (!m_deinitializer)
            {
                map_policy::erase(m, key);
                auto lk = std::unique_lock<std::mutex> { m_mutex };
                for(auto pMap : m_maps)
                    map_policy::erase(*pMap, key);
            }
            else
            {
                auto r = map_policy::find(m, key);
                if (r)
                {
                    m_deinitializer(*r);
                    map_policy::erase(m, key);
                }

                auto lk = std::unique_lock<std::mutex> { m_mutex };
                for(auto pMap : m_maps)
                {
                    auto r = map_policy::find(*pMap, key);
                    if (r)
                    {
                        m_deinitializer(*r);
                        map_policy::erase(*pMap, key);
                    }
                }
            }

            GEOMETRIX_ASSERT(!map_policy::find(m, this));

#ifdef STK_NO_CXX11_THREAD_LOCAL
            if(map_policy::empty(m))
                destroy_hive(&m);
#endif
        }

#ifdef STK_NO_CXX11_THREAD_LOCAL
        static instance_map*& access_hive();
        static void destroy_hive(instance_map* pMap)
        {
            delete pMap;
            access_hive() = nullptr;
        }
#endif

        std::function<T()> m_initializer;
        std::function<void(T&)> m_deinitializer;

        mutable std::mutex m_mutex;
        mutable std::set<instance_map*> m_maps;
#ifndef NDEBUG
        std::atomic<bool> m_isBeingDestructed { false };
#endif

    };

}}//! stk::thread.

#ifdef STK_NO_CXX11_THREAD_LOCAL
#include <stk/thread/on_thread_exit.hpp>
    #ifdef STK_DEFINE_THREAD_SPECIFIC_HIVE_INLINE
        namespace stk { namespace thread {
            template <typename T, typename Map, typename Tag>
            inline typename thread_specific<T, Map, Tag>::instance_map*& thread_specific<T, Map, Tag>::access_hive()
            {
                static STK_THREAD_LOCAL_POD instance_map* instance = nullptr;
                return instance;
            }
            template <typename T, typename Map, typename Tag>
            inline typename thread_specific<T, Map, Tag>::instance_map& thread_specific<T, Map, Tag>::hive()
            {
                auto instance = access_hive();
                if (!instance)
                {
                    instance = new instance_map{};
                    access_hive() = instance;
					stk::this_thread::on_thread_exit([]()
					{
						instance_map*& instance = access_hive();
						delete instance;
						instance = nullptr;
					});
                }
                GEOMETRIX_ASSERT(access_hive() == instance);
                return *instance;
            }
        }}//! namespace stk::thread;

        #define STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(T, Map, Tag)
    #else
        //! This should be placed in a cpp file to avoid issues with singletons and the ODR.
        //! There should be only one such definition for each type T specified.
        #define STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(T, Map, Tag)        \
        namespace stk { namespace thread {                                  \
        template <>                                                         \
        inline typename thread_specific<T, Map, Tag>::instance_map*&        \
        thread_specific<T, Map, Tag>::access_hive()                         \
        {                                                                   \
            static STK_THREAD_LOCAL_POD instance_map* instance = nullptr;   \
            return instance;                                                \
        }                                                                   \
        template <>                                                         \
        inline thread_specific<T,Map,Tag>::instance_map&                    \
        thread_specific<T,Map,Tag>::hive()                                  \
        {                                                                   \
            auto instance = access_hive();                                  \
            if (!instance)                                                  \
            {                                                               \
                instance = new instance_map{};                              \
                access_hive() = instance;                                   \
				stk::this_thread::on_thread_exit([]()                       \
				{                                                           \
					instance_map*& instance = access_hive();                \
					delete instance;                                        \
					instance = nullptr;                                     \
				});                                                         \
            }                                                               \
            GEOMETRIX_ASSERT(access_hive() == instance);                    \
            return *instance;                                               \
        }                                                                   \
        }}                                                                  \
        /***/
    #endif
#else//! thread_local keyword exists.
    #ifdef STK_DEFINE_THREAD_SPECIFIC_HIVE_INLINE
        namespace stk { namespace thread {
            template <typename T, typename Map, typename Tag>
            inline typename thread_specific<T, Map, Tag>::instance_map& thread_specific<T, Map, Tag>::hive()
            {
                static thread_local instance_map instance;
                instance_map& m = instance;
                return m;
            }
        }}//! namespace stk::thread;

        #define STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(T, Map, Tag)
    #else
        //! This should be placed in a cpp file to avoid issues with singletons and the ODR.
        //! There should be only one such definition for each type T specified.
        #define STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(T, Map, Tag)        \
        namespace stk { namespace thread {                                  \
        template <>                                                         \
        inline thread_specific<T, Map, Tag>::instance_map&                  \
        thread_specific<T, Map, Tag>::hive()                                \
        {                                                                   \
            static thread_local instance_map instance;                      \
            instance_map& m = instance;                                     \
            return m;                                                       \
        }                                                                   \
        }}                                                                  \
        /***/
    #endif
#endif//! STK_NO_CXX11_THREAD_LOCAL

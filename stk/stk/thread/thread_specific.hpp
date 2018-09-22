//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_THREAD_THREAD_SPECIFIC_HPP
#define STK_THREAD_THREAD_SPECIFIC_HPP
#pragma once

#include <stk/thread/thread_local_pod.hpp>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <type_traits>

namespace stk { namespace thread {

    template <typename T>
    class thread_specific;

    //! Creates a thread local object which can be scoped to an objects lifetime. 
    template <typename T>
    class thread_specific
    {
        using data_ptr = T*;
        using const_data_ptr = typename std::add_const<data_ptr>::type;
        struct instance_map : std::unordered_map<thread_specific<T> const*, T>
        {
            instance_map() = default;
            instance_map(const instance_map&)=delete;
            instance_map& operator=(const instance_map&)=delete;
            instance_map(instance_map&&)=delete;
            instance_map& operator=(instance_map&&)=delete;
            ~instance_map()
            {
                for (auto& i : *this)
                {
                    if (i.first->m_deinitializer)
                        i.first->m_deinitializer(i.second);
                    i.first->remove_instance_map(this);
                }
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

        explicit operator bool()
        {
            return has_value_on_calling_thread();
        }

    private:

        data_ptr get_item() const
        {
	        GEOMETRIX_ASSERT(!m_isBeingDestructed);
            auto& m = hive();
            auto iter = m.find(this);
            if (iter != m.end())
                return &iter->second;

            return nullptr;
        }

        data_ptr get_or_add_item () const
        {
	        GEOMETRIX_ASSERT(!m_isBeingDestructed);
            auto& m = hive();
            auto iter = m.find(this);
            if(iter != m.end())
                return &iter->second;

	        {
		        auto lk = std::unique_lock<std::mutex> { m_mutex };
                m_maps.insert(&m);
	        }
            iter = m.insert(iter, std::make_pair(this, m_initializer()));
            GEOMETRIX_ASSERT(iter != m.end());
            return &iter->second;
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
                m.erase(key);
	            auto lk = std::unique_lock<std::mutex> { m_mutex };
	            for(auto pMap : m_maps)
                {
                    pMap->erase(key);
                }
            }
            else
            {
                auto it = m.find(key);
                if (it != m.end())
                {
                    m_deinitializer(it->second);
                    m.erase(it);
                }
	            
	            auto lk = std::unique_lock<std::mutex> { m_mutex };
	            for(auto pMap : m_maps)
                {
                    auto it = pMap->find(key);
                    if (it != pMap->end())
                    {
                        m_deinitializer(it->second);
                        pMap->erase(it);
                    }
                }
            }

            GEOMETRIX_ASSERT(m.find(this) == m.end());
        }

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
    #ifdef STK_DEFINE_THREAD_SPECIFIC_HIVE_INLINE
        namespace stk { namespace thread {
            template <typename T>
            inline thread_specific<T>::instance_map& thread_specific<T>::hive()
            {
                static std::list<std::unique_ptr<instance_map>> deleters;
				static std::mutex m;
				static STK_THREAD_LOCAL_POD instance_map* instance = nullptr;
				if (!instance) 
				{
					auto lk = std::unique_lock<std::mutex>{ m };
					deleters.emplace_back(boost::make_unique<instance_map>());
					instance = deleters.back().get();
				}
                return *instance;
            }
        }}//! namespace stk::thread;

        #define STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(T)
    #else
        //! This should be placed in a cpp file to avoid issues with singletons and the ODR.
        //! There should be only one such definition for each type T specified.
        #define STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(T)                  \
        namespace stk { namespace thread {                                  \
        template <>                                                         \
        inline thread_specific<T>::instance_map& thread_specific<T>::hive() \
        {                                                                   \
			static std::list<std::unique_ptr<instance_map>> deleters;       \
			static std::mutex m;                                            \
			static STK_THREAD_LOCAL_POD instance_map* instance = nullptr;   \
			if (!instance)                                                  \
			{                                                               \
				auto lk = std::unique_lock<std::mutex>{ m };                \
				deleters.emplace_back(boost::make_unique<instance_map>());  \
				instance = deleters.back().get();                           \
			}                                                               \
            return *instance;                                               \
        }                                                                   \
        }}                                                                  \
        /***/
    #endif
#else//! thread_local keyword exists.
    #ifdef STK_DEFINE_THREAD_SPECIFIC_HIVE_INLINE
        namespace stk { namespace thread {
            template <typename T>
            inline thread_specific<T>::instance_map& thread_specific<T>::hive()
            {
                static thread_local instance_map instance;
                instance_map& m = instance;
                return m;
            }
        }}//! namespace stk::thread;

        #define STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(T)
    #else
        //! This should be placed in a cpp file to avoid issues with singletons and the ODR.
        //! There should be only one such definition for each type T specified.
        #define STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(T)                  \
        namespace stk { namespace thread {                                  \
        template <>                                                         \
        inline thread_specific<T>::instance_map& thread_specific<T>::hive() \
        {                                                                   \
            static thread_local instance_map instance;                      \
            instance_map& m = instance;                                     \
            return m;                                                       \
        }                                                                   \
        }}                                                                  \
        /***/
    #endif
#endif//! STK_NO_CXX11_THREAD_LOCAL

#endif //! STK_THREAD_THREAD_SPECIFIC_HPP

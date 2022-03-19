//
//! Copyright © 2022
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/container/concurrent_pointer_unordered_map.hpp>
#include <stk/utility/memory_pool.hpp>
#include <stk/utility/string_hash.hpp>

#include <atomic>
#include <cstdint>

namespace stk {
        
    struct job
	{
		enum job_state : std::uint8_t
		{
			NotStarted = 0
		,   Running    = 1
		,   Finished   = 2
		,   Aborted    = 3
		};

		job()
			: state{}
			, hash( "invalid" )
		{}
		job(const string_hash& n)
			: state{}
			, hash{n}
		{}

		template <job_state S>
		void set()
		{
			if constexpr(S == Running)
			{
				auto expected = NotStarted;
				state.compare_exchange_strong(expected, S, std::memory_order_relaxed);
			}
			if constexpr(S == Finished || S == Aborted)
			{
				auto expected = Running;
				state.compare_exchange_strong(expected, S, std::memory_order_relaxed);
			}
		}

		template <job_state S>
		bool is() const { return state.load(std::memory_order_relaxed) == S; }

	private:

		std::atomic<job_state> state;
		stk::string_hash       hash;

	};

    class job_tracker
    {
    public:
    
        job_tracker() = default;
        
        job* find_job(const string_hash& name) const
        {
            return find_job_impl(name);
        }

        job* get_job(const string_hash& name)
        {
            return get_job_impl(name);
        }

        void erase_job(const string_hash& name) 
        {
			auto key = name.hash();
            m_map.erase_direct(key);
        }

        template <typename JobFn, typename Executor>
        void invoke_job(const string_hash& name, JobFn&& fn, Executor&& executor)
        {
			executor([&, this]()
			{
				auto* j = get_job( name );
				j->set<job::Running>();
				try
				{
				    fn();
				}
                catch (...)
                {
					j->set<job::Aborted>();
					throw;
                }
				j->set<job::Finished>();
			});
        }

        void quiesce()
        {
            m_qsbr.flush();
        }

    private:
        
        using mpool_t = memory_pool<job>;
        struct pool_deleter
        {
            void operator()(job* v) const
            {
                stk::deallocate_to_pool(v);
            }
        };
        
        job* find_job_impl(const string_hash& name) const
        {
            auto key = name.hash();
            auto* j = m_map.find(key);
            return j;
        }
        
        job* get_job_impl(const string_hash& name)
        {
            auto key = name.hash();
            auto* j = m_map.find(key);
            if(j != nullptr)
                return j;

            auto [ivalue, inserted] = m_map.insert(key, std::unique_ptr<job, pool_deleter>{ mpool_t::construct(m_pool.allocate()) });
            return ivalue;
        }

        mutable mpool_t                                                          m_pool;
        mutable concurrent_pointer_unordered_map<std::size_t, job, pool_deleter, junction::QSBRMemoryReclamationPolicy> m_map;
        junction::QSBR m_qsbr;

    };

}//! namespace stk;

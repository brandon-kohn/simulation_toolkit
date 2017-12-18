
//          Copyright Oliver Kowalke 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef STK_BOOST_FIBERS_ALGO_WORK_STEALING_H
#define STK_BOOST_FIBERS_ALGO_WORK_STEALING_H

#include <atomic>
#include <condition_variable>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

#include <boost/config.hpp>
#include <boost/intrusive_ptr.hpp>

#include <boost/fiber/algo/algorithm.hpp>
#include <boost/fiber/context.hpp>
#include <boost/fiber/detail/config.hpp>
#include <boost/fiber/scheduler.hpp>
#include <boost/fiber/detail/context_spinlock_queue.hpp>
//#include <boost/fiber/detail/context_spmc_queue.hpp>
#include <boost/context/detail/prefetch.hpp>
#include <boost/fiber/type.hpp>
#include <geometrix/utility/assert.hpp>

#include <random>

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace fibers {
namespace algo {

class pool_work_stealing : public algorithm {
private:
    std::vector<intrusive_ptr<pool_work_stealing>>* schedulers_;

    std::uint32_t                                  id_;
    std::uint32_t                                  thread_count_;
	detail::context_spinlock_queue                 rqueue_{};
    std::mutex                                     mtx_{};
    std::condition_variable                        cnd_{};
    bool                                           flag_{ false };
    bool                                           suspend_;

public:
    
	pool_work_stealing(std::uint32_t id, std::vector<intrusive_ptr<pool_work_stealing>>* schedulers, bool suspend = false)
		: schedulers_(schedulers)
		, id_{ id }
		, thread_count_{ static_cast<std::uint32_t>( schedulers->size() ) }
		, suspend_{ suspend }
	{
		(*schedulers_)[id_] = this;
	}

	virtual ~pool_work_stealing()
	{}
	
	pool_work_stealing( pool_work_stealing const&) = delete;
    pool_work_stealing( pool_work_stealing &&) = delete;

    pool_work_stealing & operator=( pool_work_stealing const&) = delete;
    pool_work_stealing & operator=( pool_work_stealing &&) = delete;

    virtual void awakened(context* ctx) noexcept
	{
		if (!ctx->is_context(type::pinned_context))
			ctx->detach();
		rqueue_.push(ctx);
	}

	virtual context * steal() noexcept {
		return rqueue_.steal();
	}

    virtual bool has_ready_fibers() const noexcept {
        return !rqueue_.empty();
    }

	virtual context* pick_next() noexcept 
	{
		context * victim = rqueue_.pop();
		if (nullptr != victim) 
		{
			boost::context::detail::prefetch_range(victim, sizeof(context));
			if (!victim->is_context(type::pinned_context)) 
				context::active()->attach(victim);
		} 
		else 
		{
			std::uint32_t id = 0;
			std::size_t count = 0, size = schedulers_->size();
			static thread_local std::minstd_rand generator{ std::random_device{}() };
			std::uniform_int_distribution< std::uint32_t > distribution{0, static_cast<std::uint32_t>(thread_count_ - 1)};
			do 
			{
				do 
				{
					++count;
					// random selection of one logical cpu
					// that belongs to the local NUMA node
					id = distribution(generator);
					// prevent stealing from own scheduler
				}
				while (id == id_);
				// steal context from other scheduler
				victim = (*schedulers_)[id]->steal();
			} 
			while (nullptr == victim && count < size);
			if (nullptr != victim) 
			{
				boost::context::detail::prefetch_range(victim, sizeof(context));
				BOOST_ASSERT(!victim->is_context(type::pinned_context));
				context::active()->attach(victim);
			}
		}
		return victim;
	}

	virtual void suspend_until(std::chrono::steady_clock::time_point const& time_point) noexcept 
	{
		if (suspend_) 
		{
			if ((std::chrono::steady_clock::time_point::max)() == time_point) 
			{
				std::unique_lock< std::mutex > lk{ mtx_ };
				cnd_.wait(lk, [this]() { return flag_; });
				flag_ = false;
			}
			else 
			{
				std::unique_lock< std::mutex > lk{ mtx_ };
				cnd_.wait_until(lk, time_point, [this]() { return flag_; });
				flag_ = false;
			}
		}
	}

	virtual void notify() noexcept 
	{
		if (suspend_) 
		{
			std::unique_lock< std::mutex > lk{ mtx_ };
			flag_ = true;
			lk.unlock();
			cnd_.notify_all();
		}
	}
};

}}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif

#endif // STK_BOOST_FIBERS_ALGO_WORK_STEALING_H

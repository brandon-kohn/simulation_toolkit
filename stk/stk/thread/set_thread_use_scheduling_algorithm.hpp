#pragma once

#include <boost/fiber/algo/round_robin.hpp>
#include <boost/fiber/algo/shared_work.hpp>
#include <boost/fiber/algo/work_stealing.hpp>
//#include <boost/fiber/algo/numa/work_stealing.hpp>

#include <stk/thread/pool_work_stealing.hpp>

namespace stk { namespace thread {

    struct fiber_scheduler_args
    {
        std::uint32_t   nThreads;
		bool			suspend{ false };
    };

    template <typename Algo, typename EnableIf=void>
    struct fiber_scheduler_thread_assigner
    {};

    template <>
    struct fiber_scheduler_thread_assigner<boost::fibers::algo::round_robin>
    {
        void operator()(fiber_scheduler_args const&)
        {
            boost::fibers::use_scheduling_algorithm<boost::fibers::algo::round_robin>();
        }
    };

    template <>
    struct fiber_scheduler_thread_assigner<boost::fibers::algo::work_stealing>
    {
        void operator()(fiber_scheduler_args const& a)
        {
            boost::fibers::use_scheduling_algorithm<boost::fibers::algo::work_stealing>(a.nThreads, a.suspend);
        }
    };

    template <>
    struct fiber_scheduler_thread_assigner<boost::fibers::algo::shared_work>
    {
        void operator()(fiber_scheduler_args const& a)
        {
            boost::fibers::use_scheduling_algorithm<boost::fibers::algo::shared_work>(a.nThreads);
        }
    };

	struct work_stealing_fiber_scheduler_args
	{
		work_stealing_fiber_scheduler_args() = default;

		work_stealing_fiber_scheduler_args(std::uint32_t id, std::vector<boost::intrusive_ptr<boost::fibers::algo::pool_work_stealing>>* schedulers = nullptr, bool suspend = false )
			: id(id)
			, schedulers(schedulers)
			, suspend(suspend)
		{}

		std::uint32_t id{ static_cast<std::uint32_t>(-1) };
		std::vector<boost::intrusive_ptr<boost::fibers::algo::pool_work_stealing>>* schedulers{ nullptr };
		bool			suspend{ false };
	};

	template <>
	struct fiber_scheduler_thread_assigner<boost::fibers::algo::pool_work_stealing>
	{
		void operator()(work_stealing_fiber_scheduler_args const& a)
		{
			boost::fibers::use_scheduling_algorithm<boost::fibers::algo::pool_work_stealing>(a.id, a.schedulers, a.suspend);
		}
	};

    template <typename Algo, typename Args>
    inline void set_thread_use_scheduling_algorithm(const Args& args)
    {
        fiber_scheduler_thread_assigner<Algo>()(args);
    }

}}//! namespace stk::thread;


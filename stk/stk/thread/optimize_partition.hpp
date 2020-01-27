#pragma once

#include <stk/thread/partition_work.hpp>
#include <stk/utility/time_execution.hpp>

namespace stk {

	namespace detail {
		template <typename Pool, typename Jobs>
		inline std::chrono::duration<double> measure_partition(Pool&& pool, Jobs const& jobs, std::size_t npartitions)
		{
			return time_execution([&](){pool.parallel_for(jobs, npartitions);});
		}

		template <typename Pool, typename Job>
		inline std::chrono::duration<double> measure_partition(Pool&& pool, std::size_t nInvokations, Job const& job, std::size_t npartitions)
		{
			return time_execution([&](){pool.parallel_apply(nInvokations, job, npartitions);});
		}
	}

	template <typename Pool, typename Jobs, typename Step>
	inline std::size_t optimize_partition(Pool&& pool, const Jobs& jobs, std::size_t npartitions, Step const& step)
	{
		auto njobs = jobs.size();
		auto nThreads = pool.number_threads();
		if (njobs <= nThreads)
			return njobs;

		auto minDuration = detail::measure_partition(pool, jobs, npartitions);
		auto trial = step(npartitions);
		while (trial <= njobs)
		{
			auto tmin = detail::measure_partition(pool, jobs, trial);
			if (tmin < minDuration) {
				npartitions = trial;
				minDuration = tmin;
				trial = step(npartitions);
			} else {
				break;
			}
		}

		return npartitions;
	}
	
	template <typename Pool, typename Job, typename Step>
	inline std::size_t optimize_partition(Pool&& pool, std::size_t nInvokations, const Job& job, std::size_t npartitions, Step const& step)
	{
		auto nThreads = pool.number_threads();
		if (nInvokations <= nThreads)
			return nInvokations;

		auto minDuration = detail::measure_partition(pool, nInvokations, job, npartitions);
		auto trial = step(npartitions);
		while (trial <= nInvokations)
		{
			auto tmin = detail::measure_partition(pool, nInvokations, job, trial);
			if (tmin < minDuration) {
				npartitions = trial;
				minDuration = tmin;
				trial = step(npartitions);
			} else {
				break;
			}
		}

		return npartitions;
	}


}//! namespace stk;
 

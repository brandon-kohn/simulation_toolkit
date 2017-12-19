#include <stk/thread/pool_work_stealing.hpp>
#include <boost/context/detail/prefetch.hpp>

namespace boost { namespace fibers { namespace algo {

boost::fibers::context* pool_work_stealing::pick_next() noexcept
{
	context * victim = rqueue_.pop();
	if (nullptr != victim)
	{
		boost::context::detail::prefetch_range(victim, sizeof(context));
		if (!victim->is_context(type::pinned_context))
			context::active()->attach(victim);
	} else
	{
		std::uint32_t id = 0;
		std::size_t count = 0, size = schedulers_->size();
		static thread_local std::minstd_rand generator{ std::random_device{}() };
		std::uniform_int_distribution< std::uint32_t > distribution{ 0, static_cast<std::uint32_t>(thread_count_ - 1) };
		do
		{
			do
			{
				++count;
				// random selection of one logical cpu
				// that belongs to the local NUMA node
				id = distribution(generator);
				// prevent stealing from own scheduler
			} while (id == id_);
			// steal context from other scheduler
			victim = (*schedulers_)[id]->steal();
		} while (nullptr == victim && count < size);
		if (nullptr != victim)
		{
			boost::context::detail::prefetch_range(victim, sizeof(context));
			BOOST_ASSERT(!victim->is_context(type::pinned_context));
			context::active()->attach(victim);
		}
	}
	return victim;
}

}}}//! namespace boost::fibers::algo;

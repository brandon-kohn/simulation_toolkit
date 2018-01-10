//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <geometrix/numeric/number_comparison_policy.hpp>
#include <geometrix/primitive/point.hpp>
#include <geometrix/algorithm/grid_traits.hpp>
#include <geometrix/algorithm/grid_2d.hpp>
#include <geometrix/algorithm/hash_grid_2d.hpp>
#include <geometrix/algorithm/orientation_grid_traversal.hpp>
#include <geometrix/algorithm/fast_voxel_grid_traversal.hpp>
#include <geometrix/primitive/segment.hpp>
#include <junction/ConcurrentMap_Leapfrog.h>
#include <stk/thread/active_object.hpp>
#include <stk/thread/thread_pool.hpp>
#include <stk/thread/work_stealing_thread_pool.hpp>
#include <stk/thread/fiber_pool.hpp> //-> requires C++11 with constexpr/noexcept etc. vs140 etc.
#include <stk/thread/tiny_atomic_spin_lock.hpp>
#include <stk/random/xorshift1024starphi_generator.hpp>
#include <stk/geometry/geometry_kernel.hpp>
#include <junction/ConcurrentMap_Leapfrog.h>
#include <boost/thread/futures/wait_for_all.hpp>
#include <boost/context/stack_traits.hpp>
#include <geometrix/utility/scope_timer.ipp>
#include <stk/thread/concurrentqueue.h>
#include <stk/utility/compressed_integer_pair.hpp>
#include <junction/Core.h>
#include <turf/Util.h>
#include <chrono>

STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(std::uint32_t);

namespace stk {

	template <typename Fn, typename ... Ts>
	inline std::chrono::duration<double> time_execution(Fn&& fn, Ts&&... args)
	{
		auto start = std::chrono::high_resolution_clock::now();
		fn(std::forward<Ts>(args)...);
		auto end = std::chrono::high_resolution_clock::now();
		return std::chrono::duration<double>{ end - start };
	}

	struct compressed_integer_pair_key_traits 
	{
		typedef std::uint64_t Key;
		typedef typename turf::util::BestFit<std::uint64_t>::Unsigned Hash;
		static const Key NullKey = (std::numeric_limits<Key>::max)();
		static const Hash NullHash = Hash((std::numeric_limits<Key>::max)());
		static Hash hash(Key key) 
		{
			return turf::util::avalanche(Hash(key));
		}
		static Key dehash(Hash hash) 
		{
			return (Key)turf::util::deavalanche(hash);
		}
	};

	template <typename T>
	struct pointer_value_traits 
	{
		typedef T* Value;
		typedef typename turf::util::BestFit<T*>::Unsigned IntType;
		static const IntType NullValue = 0;
		static const IntType Redirect = 1;
	};

}//! namespace stk;

template<typename Data, typename GridTraits>
class concurrent_hash_grid_2d
{
public:

	typedef Data* data_ptr;
	typedef GridTraits traits_type;
	typedef stk::compressed_integer_pair key_type;
	typedef junction::ConcurrentMap_Leapfrog<std::uint64_t, Data*, stk::compressed_integer_pair_key_traits, stk::pointer_value_traits<Data>> grid_type;

	concurrent_hash_grid_2d( const GridTraits& traits )
		: m_gridTraits(traits)
		, m_grid()
	{}

	~concurrent_hash_grid_2d()
	{
		quiesce();
		clear();
	}
	
	template <typename Point>
	data_ptr find_cell(const Point& point) const
	{
		BOOST_CONCEPT_ASSERT( (Point2DConcept<Point>) );
		GEOMETRIX_ASSERT( is_contained( point ) );
		boost::uint32_t i = m_gridTraits.get_x_index(get<0>(point));
		boost::uint32_t j = m_gridTraits.get_y_index(get<1>(point));
		return find_cell(i,j);
	}
	
	data_ptr find_cell(boost::uint32_t i, boost::uint32_t j) const
	{    
		stk::compressed_integer_pair p = { i, j };
		auto iter = m_grid.find(p.to_uint64());
		GEOMETRIX_ASSERT(iter.getValue() != (data_ptr)stk::pointer_value_traits<Data>::Redirect);
		return iter.getValue();
	}
	
	template <typename Point>
	Data& get_cell(const Point& point)
	{
		BOOST_CONCEPT_ASSERT(( Point2DConcept<Point> ));
		GEOMETRIX_ASSERT( is_contained( point ) );
		boost::uint32_t i = m_gridTraits.get_x_index(get<0>(point));
		boost::uint32_t j = m_gridTraits.get_y_index(get<1>(point));
		return get_cell(i,j);
	}
	
	Data& get_cell(boost::uint32_t i, boost::uint32_t j)
	{            
		stk::compressed_integer_pair p = { i, j };
		auto mutator = m_grid.insertOrFind(p.to_uint64());
		auto result = mutator.getValue();
		if (result == nullptr)
		{
			auto pNewData = std::make_unique<Data>();
			result = pNewData.get();
			if (result != mutator.exchangeValue(result))
				pNewData.release();
			result = mutator.getValue();
		}

		GEOMETRIX_ASSERT(result != nullptr);
		return *result;
	}
	
	const traits_type& get_traits() const { return m_gridTraits; }

	template <typename Point>
	bool is_contained( const Point& p ) const
	{
		BOOST_CONCEPT_ASSERT( (Point2DConcept<Point>) );
		return m_gridTraits.is_contained( p );
	}

	void erase(boost::uint32_t i, boost::uint32_t j)
	{
		compressed_integer_pair p = { i, j };
		auto iter = m_grid.find(p.to_uint64());
		if (iter.isValid())
			iter.eraseValue();
	}

	void clear()
	{
		auto it = grid_type::Iterator(m_grid);
		while(it.isValid())
		{
			auto pValue = it.getValue();
			delete pValue;
			m_grid.erase(it.getKey());
			it.next();
		};
	}
	
	//! This should be called when the grid is not being modified to allow threads to reclaim memory from deletions.
	void quiesce()
	{
		junction::DefaultQSBR.flush();
	}

private:

	traits_type m_gridTraits;
	mutable grid_type m_grid;

};

class null_mutex
{
public:

	null_mutex() = default;

	void lock()
	{

	}

	bool try_lock()
	{
		return true;
	}

	void unlock()
	{
	}
};

template <typename Mutex>
struct cell
{
	static std::int64_t update(int i)
	{
		static std::atomic<std::int64_t> c = 0;
		return c.fetch_add(i, std::memory_order_relaxed);
	}

	cell()
	{
		update(1);
	}

	~cell()
	{
		update(-1);
	}

	void set_position(const stk::point2& p)
	{
		auto lk = std::unique_lock<Mutex>{ mtx };
		pos = p;
	}

	int id{ -1 };
	stk::point2 pos;
	Mutex mtx;
};

int extent = 20000;
TEST(concurrent_hash_grid_2d_tests, construct_and_delete)
{
	using namespace geometrix;
	using namespace stk;
	using namespace stk::thread;
	{
		double xmin = 0;
		double xmax = extent;
		double ymin = 0;
		double ymax = extent;
		grid_traits<double> grid(xmin, xmax, ymin, ymax, 3.0);
		concurrent_hash_grid_2d<cell<null_mutex>, grid_traits<double>> sut(grid);
		
		for (int i = 0; i < extent; ++i)
			sut.get_cell(i, i);
		EXPECT_EQ(extent, cell<null_mutex>::update(0));
	}

	EXPECT_EQ(0, cell<null_mutex>::update(0));
}
#include <boost/range/iterator.hpp>
template <typename Container>
inline auto partition_work(Container& cont, const std::ptrdiff_t num)
{
	using iterator = typename boost::range_iterator<Container>::type;
	auto first = std::begin(cont);
	auto last = std::end(cont);
	auto total = std::distance(first,last);
	auto portion = std::ptrdiff_t{ total / num };
	std::vector<boost::iterator_range<iterator>> chunks(num);
	auto portion_end = first;
	std::generate(std::begin(chunks), std::end(chunks), [&portion_end, portion]()
	{
		auto portion_start = portion_end;
		std::advance(portion_end, portion);
		return boost::make_iterator_range(portion_start, portion_end);
	});
	
	chunks.back() = boost::make_iterator_range(std::begin(chunks.back()), last);
	return chunks;
}

template <typename Mutex, typename Pool, typename Inputs>
void bash_grid(Pool& pool, Inputs const& rndpairs, std::string const& name, std::size_t partitionSize)
{
	using namespace geometrix;
	using namespace stk;
	using namespace stk::thread;

	double xmin = 0;
	double xmax = extent;
	double ymin = 0;
	double ymax = extent;
	grid_traits<double> grid(xmin, xmax, ymin, ymax, 3.0);
	concurrent_hash_grid_2d<cell<Mutex>, grid_traits<double>> sut(grid);

	using future_t = typename Pool::template future<void>;
	std::vector<future_t> fs;

	auto partitions = partition_work(rndpairs, partitionSize);
	GEOMETRIX_ASSERT(partitions.size() == partitionSize);
	fs.reserve(partitions.size());
	{
		GEOMETRIX_MEASURE_SCOPE_TIME(name);
		for (auto const& items : partitions)
		{
			fs.emplace_back(pool.send([&sut, &items]() -> void {
				for (auto p : items)
				{
					auto& c = sut.get_cell(p.first, p.second);
					c.set_position(stk::point2(p.first * boost::units::si::meters, p.second * boost::units::si::meters));
				}
			}));
		}
		boost::for_each(fs, [](const future_t& f) { f.wait(); });
	}

	auto cmp = make_tolerance_policy();
	for (auto p : rndpairs)
	{
		auto i = p.first;
		auto j = p.second;
		auto pCell = sut.find_cell(i, j);
		ASSERT_NE(nullptr, pCell);
		EXPECT_TRUE(geometrix::numeric_sequence_equals(pCell->pos, stk::point2(p.first * boost::units::si::meters, p.second * boost::units::si::meters), cmp));
	}
}

template <typename Mutex, typename Pool, typename Inputs>
void bash_grid_with_striping(Pool& pool, Inputs const& rndpairs, std::string const& name, std::size_t partitionSize)
{
	using namespace geometrix;
	using namespace stk;
	using namespace stk::thread;

	double xmin = 0;
	double xmax = extent;
	double ymin = 0;
	double ymax = extent;
	grid_traits<double> grid(xmin, xmax, ymin, ymax, 3.0);
	concurrent_hash_grid_2d<cell<Mutex>, grid_traits<double>> sut(grid);

	using future_t = typename Pool::template future<void>;
	std::vector<future_t> fs;
	
	auto partitions = partition_work(rndpairs, partitionSize);
	GEOMETRIX_ASSERT(partitions.size() == partitionSize);
	fs.reserve(partitions.size());
	{
		GEOMETRIX_MEASURE_SCOPE_TIME(name);
		int idx = 0;
		for (auto const& items : partitions)
		{
			fs.emplace_back(pool.send(idx++ % pool.number_threads(), [&sut, &items]() -> void {
				for (auto p : items)
				{
					auto& c = sut.get_cell(p.first, p.second);
					c.set_position(stk::point2(p.first * boost::units::si::meters, p.second * boost::units::si::meters));
				}
			}));
		}
		boost::for_each(fs, [](const future_t& f) { f.wait(); });
	}

	auto cmp = make_tolerance_policy();
	for (auto p : rndpairs)
	{
		auto i = p.first;
		auto j = p.second;
		auto pCell = sut.find_cell(i, j);
		ASSERT_NE(nullptr, pCell);
		EXPECT_TRUE(geometrix::numeric_sequence_equals(pCell->pos, stk::point2(p.first * boost::units::si::meters, p.second * boost::units::si::meters), cmp));
	}
}

unsigned nTimingRuns = 20;
int nAccesses = 1000000;
std::size_t nFibersPerThread = 15;
std::size_t nOSThreads = std::thread::hardware_concurrency() - 1;
std::size_t sPartitions = nOSThreads * 100;
TEST(timing, fibers_moodycamel_concurrentQ_bash_grid)
{
	using namespace geometrix;
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	auto alloc = boost::fibers::fixedsize_stack{ 64 * 1024 };
	fiber_pool<boost::fibers::fixedsize_stack, moodycamel_concurrent_queue_traits> fibers{nFibersPerThread, alloc};
	std::vector<std::pair<std::uint32_t, std::uint32_t>> rndpairs;
	xorshift1024starphi_generator gen;

	for (int i = 0; i < nAccesses; ++i)
		rndpairs.emplace_back(gen() % extent, gen() % extent);
	
	for (int i = 0; i < nTimingRuns; ++i)
		bash_grid<tiny_atomic_spin_lock<eager_fiber_yield_wait<5000>>>(fibers, rndpairs, "fibers_bash_grid",sPartitions);

	EXPECT_TRUE(true);
}

#include <stk/thread/work_stealing_fiber_pool.hpp>
TEST(timing, work_stealing_fibers_moodycamel_concurrentQ_bash_grid)
{
	using namespace geometrix;
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	auto alloc = boost::fibers::fixedsize_stack{ 64 * 1024 };
	work_stealing_fiber_pool<boost::fibers::fixedsize_stack, moodycamel_concurrent_queue_traits> fibers{nFibersPerThread, alloc};
	std::vector<std::pair<std::uint32_t, std::uint32_t>> rndpairs;
	xorshift1024starphi_generator gen;

	for (int i = 0; i < nAccesses; ++i)
		rndpairs.emplace_back(gen() % extent, gen() % extent);
	
	for (int i = 0; i < nTimingRuns; ++i)
		bash_grid_with_striping<tiny_atomic_spin_lock<eager_fiber_yield_wait<5000>>>(fibers, rndpairs, "work_stealing_fibers_bash_grid_with_striping",sPartitions);

	EXPECT_TRUE(true);
}
/*
#include <stk/thread/fiber_thread_system.hpp>
#include "pool_work_stealing.hpp"
TEST(timing, fibers_thread_system_bash_grid)
{
	using namespace geometrix;
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	auto salloc = boost::fibers::fixedsize_stack{ 64 * 1024 };
	std::vector<boost::intrusive_ptr<boost::fibers::algo::pool_work_stealing>> schedulers(std::thread::hardware_concurrency());

	auto schedulerPolicy = [&schedulers](unsigned int id)
	{
		boost::fibers::use_scheduling_algorithm<boost::fibers::algo::pool_work_stealing>(id, &schedulers, false);
	};
	fiber_thread_system<decltype(salloc)> fts(salloc, std::thread::hardware_concurrency(), schedulerPolicy);
	std::vector<std::pair<std::uint32_t, std::uint32_t>> rndpairs;
	xorshift1024starphi_generator gen;

	for (int i = 0; i < nAccesses; ++i)
		rndpairs.emplace_back(gen() % extent, gen() % extent);
	using namespace ::testing;

	for (int i = 0; i < nTimingRuns; ++i)
		bash_grid<tiny_atomic_spin_lock<eager_fiber_yield_wait<5000>>>(fts, rndpairs, "fiber_thread_system_bash_grid",sPartitions);

	EXPECT_TRUE(true);
}
*/
TEST(timing, threads_moodycamel_concurrentQ_bash_grid)
{
	using namespace stk;
	using namespace stk::thread;

	thread_pool<moodycamel_concurrent_queue_traits> threads(nOSThreads);
	std::vector<std::pair<std::uint32_t, std::uint32_t>> rndpairs;
	xorshift1024starphi_generator gen;

	for (int i = 0; i < nAccesses; ++i)
		rndpairs.emplace_back(gen() % extent, gen() % extent);

	for (int i = 0; i < nTimingRuns; ++i)
		bash_grid<tiny_atomic_spin_lock<eager_boost_thread_yield_wait<5000>>>(threads, rndpairs, "threads_bash_grid",sPartitions);

	EXPECT_TRUE(true);
}

TEST(timing, work_stealing_threads_moodycamel_concurrentQ_bash_grid)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;

	work_stealing_thread_pool<moodycamel_concurrent_queue_traits> threads(nOSThreads);
	std::vector<std::pair<std::uint32_t, std::uint32_t>> rndpairs;
	xorshift1024starphi_generator gen;

	for (int i = 0; i < nAccesses; ++i)
		rndpairs.emplace_back(gen() % extent, gen() % extent);

	for (int i = 0; i < nTimingRuns; ++i)
		bash_grid_with_striping<tiny_atomic_spin_lock<eager_boost_thread_yield_wait<5000>>>(threads, rndpairs, "work_stealing_threads_bash_grid_with_striping",sPartitions);

	EXPECT_TRUE(true);
}

TEST(timing, work_stealing_threads_moodycamel_concurrentQ_bash_grid_wait_mode)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;

	work_stealing_thread_pool<moodycamel_concurrent_queue_traits> threads(nOSThreads);
	threads.set_polling_mode(polling_mode::wait);
	std::vector<std::pair<std::uint32_t, std::uint32_t>> rndpairs;
	xorshift1024starphi_generator gen;

	for (int i = 0; i < nAccesses; ++i)
		rndpairs.emplace_back(gen() % extent, gen() % extent);

	for (int i = 0; i < nTimingRuns; ++i)
		bash_grid_with_striping<tiny_atomic_spin_lock<eager_boost_thread_yield_wait<5000>>>(threads, rndpairs, "work_stealing_threads_bash_grid_with_striping_wait_mode",sPartitions);

	EXPECT_TRUE(true);
}

template <typename Inputs>
void bash_sequential_grid(Inputs const& rndpairs, std::string const& name)
{
	using namespace geometrix;
	using namespace stk;
	using namespace stk::thread;

	double xmin = 0;
	double xmax = extent;
	double ymin = 0;
	double ymax = extent;
	grid_traits<double> grid(xmin, xmax, ymin, ymax, 3.0);
	hash_grid_2d<cell<null_mutex>, grid_traits<double>> sut(grid);

	{
		GEOMETRIX_MEASURE_SCOPE_TIME(name);
		for (auto p : rndpairs)
		{
			auto& c = sut.get_cell(p.first, p.second);
			c.set_position(stk::point2(p.first * boost::units::si::meters, p.second * boost::units::si::meters));
		}
	}

	auto cmp = make_tolerance_policy();
	for (auto p : rndpairs)
	{
		auto i = p.first;
		auto j = p.second;
		auto pCell = sut.find_cell(i, j);
		ASSERT_NE(nullptr, pCell);
		EXPECT_TRUE(geometrix::numeric_sequence_equals(pCell->pos, stk::point2(p.first * boost::units::si::meters, p.second * boost::units::si::meters), cmp));
	}
}

TEST(timing, sequential_bash_grid)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;

	std::vector<std::pair<std::uint32_t, std::uint32_t>> rndpairs;
	xorshift1024starphi_generator gen;

	for (int i = 0; i < nAccesses; ++i)
		rndpairs.emplace_back(gen() % extent, gen() % extent);

	for (int i = 0; i < nTimingRuns; ++i)
		bash_sequential_grid(rndpairs, "sequential_bash_grid");

	EXPECT_TRUE(true);
}

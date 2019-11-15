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
#include <stk/thread/active_object.hpp>
#include <stk/thread/thread_pool.hpp>
#include <stk/thread/work_stealing_thread_pool.hpp>
#include <stk/thread/tiny_atomic_spin_lock.hpp>
#include <stk/thread/boost_yield_wait_strategies.hpp>
#include <stk/random/xorshift1024starphi_generator.hpp>
#include <stk/geometry/geometry_kernel.hpp>
#include <boost/thread/futures/wait_for_all.hpp>
#include <boost/context/stack_traits.hpp>
#include <geometrix/utility/scope_timer.ipp>
#include <stk/thread/concurrentqueue.h>
#include <stk/thread/concurrentqueue_queue_info_no_tokens.h>
#include <stk/container/concurrent_hash_grid.hpp>
#include <chrono>

using mc_queue_traits = moodycamel_concurrent_queue_traits_no_tokens;
namespace stk {

    template <typename Fn, typename ... Ts>
    inline std::chrono::duration<double> time_execution(Fn&& fn, Ts&&... args)
    {
        auto start = std::chrono::high_resolution_clock::now();
        fn(std::forward<Ts>(args)...);
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>{ end - start };
    }
}//! namespace stk;

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
        static std::atomic<std::int64_t> c{0};
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
	junction::DefaultQSBR().flush();
    EXPECT_EQ(0, cell<null_mutex>::update(0));
}

TEST(concurrent_hash_grid_2d_tests, construct_and_erase)
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
		for (int i = 0; i < extent; ++i)
			sut.erase(i, i);
		junction::DefaultQSBR().flush();
		EXPECT_EQ(0, cell<null_mutex>::update(0));
	}
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
void bash_grid_with_striping(Pool& pool, Inputs const& rndpairs, std::string const& name, std::size_t /*partitionSize*/)
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

    {
        GEOMETRIX_MEASURE_SCOPE_TIME(name);
		pool.parallel_for(rndpairs,
			[&sut](const typename Inputs::value_type& p) -> void {
                auto& c = sut.get_cell(p.first, p.second);
                c.set_position(stk::point2(p.first * boost::units::si::meters, p.second * boost::units::si::meters));
            }
		);
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

unsigned nTimingRuns = 20UL;
auto nAccesses = 1000000UL;
std::uint32_t nOSThreads = std::thread::hardware_concurrency() - 1;
std::size_t sPartitions = nOSThreads * 100;

TEST(timing, threads_moodycamel_concurrentQ_bash_grid)
{
    using namespace stk;
    using namespace stk::thread;

    thread_pool<mc_queue_traits> threads(nOSThreads);
    std::vector<std::pair<std::uint32_t, std::uint32_t>> rndpairs;
    xorshift1024starphi_generator gen;

    for (auto i = 0UL; i < nAccesses; ++i)
        rndpairs.emplace_back(gen() % extent, gen() % extent);

    for (auto i = 0UL; i < nTimingRuns; ++i)
        bash_grid<tiny_atomic_spin_lock<eager_boost_thread_yield_wait<5000>>>(threads, rndpairs, "threads_bash_grid",sPartitions);

    EXPECT_TRUE(true);
}

TEST(timing, work_stealing_threads_moodycamel_concurrentQ_bash_grid_with_striping)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;

    work_stealing_thread_pool<mc_queue_traits> threads(nOSThreads);
    std::vector<std::pair<std::uint32_t, std::uint32_t>> rndpairs;
    xorshift1024starphi_generator gen;

    for (auto i = 0UL; i < nAccesses; ++i)
        rndpairs.emplace_back(gen() % extent, gen() % extent);

    for (auto i = 0UL; i < nTimingRuns; ++i)
        bash_grid_with_striping<tiny_atomic_spin_lock<eager_boost_thread_yield_wait<5000>>>(threads, rndpairs, "work_stealing_threads_bash_grid_with_striping",sPartitions);

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

    for (auto i = 0UL; i < nAccesses; ++i)
        rndpairs.emplace_back(gen() % extent, gen() % extent);

    for (auto i = 0UL; i < nTimingRuns; ++i)
        bash_sequential_grid(rndpairs, "sequential_bash_grid");

    EXPECT_TRUE(true);
}

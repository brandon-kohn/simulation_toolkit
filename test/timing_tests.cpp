//
//! Copyright © 2018
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include <limits>
#include <unordered_set>
#include <set>
#include <random>
#include <chrono>
#include <geometrix/utility/scope_timer.ipp>

auto nTimingRuns = 200000UL;
auto numberToInsert = 30UL;
template <typename Key>
Key random_value()
{
    static std::mt19937 eng;
    static std::uniform_int_distribution<Key> dis(0, (std::numeric_limits<Key>::max)());
    return dis(eng);
}

template <typename Key>
Key* random_pointer()
{
    static size_t maxMem = 10 * 1024 * 1024;
    static char mem[1];
    static std::mt19937 eng;
    static std::uniform_int_distribution<Key> dis(0, maxMem);
    return reinterpret_cast<Key*>(&mem[dis(eng)]);
}
template <typename T>
struct inserter
{
    template <typename U>
    static void apply(T&& c, U&& v)
    {
        c.insert(v);
    }
};

template <typename T>
struct eraser
{
    template <typename U>
    static void apply(T&& c, U&& v)
    {
        c.erase(v);
    }
};

template <typename Container, typename RangeToInsert>
void run_timing_insert(Container&& initial, RangeToInsert&& valuesToInsert, const char* name)
{
    Container c = initial;//make a copy and add to it for timings.
    {
        GEOMETRIX_MEASURE_SCOPE_TIME(name);
        for(auto&& i : valuesToInsert)
        {
            inserter<Container>::apply(c, i);
        }
    }
}

template <typename Container, typename RangeToInsert>
void run_timing_erase(Container&& initial, RangeToInsert&& valuesToErase, const char* name)
{
    using namespace ::testing;

    Container c = initial;//make a copy and add to it for timings.
    {
        GEOMETRIX_MEASURE_SCOPE_TIME(name);
        for(auto&& i : valuesToErase)
        {
            eraser<Container>::apply(c, i);
        }

        EXPECT_THAT(c, IsEmpty());
    }
}

TEST(timing, std_unordered_set_pointers_insert)
{
    using namespace ::testing;

    auto name = std::stringstream{};
    name << "insert " << numberToInsert << " items to std::unordered_set<std::uint64_t>";

    for (auto q = 0UL; q < nTimingRuns; ++q)
    {
        auto c = std::unordered_set<std::uint64_t*>{};
        auto toInsert = std::vector<std::uint64_t*>{};
        for (auto i = 0UL; i < numberToInsert; ++i)
            toInsert.push_back(random_pointer<std::uint64_t>());
        run_timing_insert(c, toInsert, name.str().c_str());
    }
}

TEST(timing, std_unordered_set_pointers_erase)
{
    using namespace ::testing;

    auto name = std::stringstream{};
    name << "erase " << numberToInsert << " items from std::unordered_set<std::uint64_t>";

    for (auto q = 0UL; q < nTimingRuns; ++q)
    {
        auto c = std::unordered_set<std::uint64_t*>{};
        auto toErase = std::vector<std::uint64_t*>{};
        for (auto i = 0UL; i < numberToInsert; ++i)
        {
            toErase.push_back(random_pointer<std::uint64_t>());
            c.insert(toErase.back());
        }
        run_timing_insert(c, toErase, name.str().c_str());
    }
}

TEST(timing, std_set_pointers_insert)
{
    using namespace ::testing;
    auto name = std::stringstream{};
    name << "insert " << numberToInsert << " items to std::set<std::uint64_t>";

    for (auto q = 0UL; q < nTimingRuns; ++q)
    {
        auto c = std::set<std::uint64_t*>{};
        auto toInsert = std::vector<std::uint64_t*>{};
        for (auto i = 0UL; i < numberToInsert; ++i)
            toInsert.push_back(random_pointer<std::uint64_t>());

        run_timing_insert(c, toInsert, name.str().c_str());
    }
}

TEST(timing, std_set_pointers_erase)
{
    using namespace ::testing;

    auto name = std::stringstream{};
    name << "erase " << numberToInsert << " items from std::set<std::uint64_t>";

    for (auto q = 0UL; q < nTimingRuns; ++q)
    {
        auto c = std::set<std::uint64_t*>{};
        auto toErase = std::vector<std::uint64_t*>{};
        for (auto i = 0UL; i < numberToInsert; ++i)
        {
            toErase.push_back(random_pointer<std::uint64_t>());
            c.insert(toErase.back());
        }
        run_timing_erase(c, toErase, name.str().c_str());
    }
}

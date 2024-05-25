//
//! Copyright © 2019
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/thread/race_detector.hpp>
#include <mutex>

#include <atomic>
#include <boost/preprocessor/cat.hpp>
#include <geometrix/utility/assert.hpp>

namespace stk::thread {

struct rw_counter 
{
	std::uint32_t readers = 0;
	std::uint32_t writers = 0;

	rw_counter add_reader()
	{
		rw_counter n = { readers + 1, writers };
		return n;
	}
	rw_counter subtract_reader()
	{
		rw_counter n = { readers - 1, writers };
		return n;
	}
	rw_counter add_writer()
	{
		rw_counter n = { readers, writers + 1 };
		return n;
	}
	rw_counter subtract_writer()
	{
		rw_counter n = { readers, writers - 1 };
		return n;
	}
};

struct rw_race_detector
{
	rw_race_detector()
		: inUse{ rw_counter{} }
	{
	}

	std::atomic<rw_counter> inUse;
};

struct read_race_guard
{
	read_race_guard( rw_race_detector& d )
		: detector( d )
	{
		auto expected = rw_counter{};
		auto hasAsserted = false;
		while( !detector.inUse.compare_exchange_weak( expected, expected.add_reader() ) ) {
			GEOMETRIX_ASSERT( hasAsserted || expected.writers == 0 );
			if( expected.writers ) {
				hasAsserted = true;
			}
		}
	}

	~read_race_guard()
	{
		rw_counter expected = { 1, 0 };
		//! assert only once.
		auto hasAsserted = false;
		while( !detector.inUse.compare_exchange_weak( expected, expected.subtract_reader() ) ) {
			GEOMETRIX_ASSERT( hasAsserted || expected.writers == 0 );
			if( expected.writers ) {
				hasAsserted = true;
			}
		}
	}

	rw_race_detector& detector;
};

struct write_race_guard
{
	write_race_guard( rw_race_detector& d )
		: detector( d )
	{
		auto expected = rw_counter{};
		auto hasAsserted = false;
		while( !detector.inUse.compare_exchange_weak( expected, expected.add_writer() ) ) {
			GEOMETRIX_ASSERT( hasAsserted || (expected.readers == 0 && expected.writers == 0 ) );
			if( expected.writers || expected.readers) {
				hasAsserted = true;
			}
		}
	}

	~write_race_guard()
	{
		rw_counter expected = { 0, 1 };
		auto hasAsserted = false;
		while( !detector.inUse.compare_exchange_weak( expected, expected.subtract_writer() ) ) {
			GEOMETRIX_ASSERT( hasAsserted || ( expected.readers == 0 && expected.writers == 1 ) );
			if( expected.writers != 1 || expected.readers ) {
				hasAsserted = true;
			}
		}
	}

	rw_race_detector& detector;
};

} // namespace stk::thread

#ifndef NDEBUG
#	define STK_RW_RACE_DETECTOR( name ) stk::thread::rw_race_detector name
#	define STK_READ_DETECT_RACE( name ) stk::thread::read_race_guard BOOST_PP_CAT( _____stk_____, BOOST_PP_CAT( name, __LINE__ ) )( name )
#	define STK_WRITE_DETECT_RACE( name ) stk::thread::write_race_guard BOOST_PP_CAT( _____stk_____, BOOST_PP_CAT( name, __LINE__ ) )( name )
#else
#	define STK_RW_RACE_DETECTOR( name )
#	define STK_READ_DETECT_RACE( name )
#	define STK_WRITE_DETECT_RACE( name )
#endif

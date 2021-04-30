#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <stk/thread/message_queue.hpp>

using namespace stk;

TEST( message_queue_test_suite, construct )
{
	auto sut = message_queue<int>{};
}

TEST( message_queue_test_suite, add_10_remove_10 )
{
	auto sut = message_queue<int>{};

	for( auto i = 0u; i < 10; ++i )
		sut.send( i );

	for( auto i = 0u; i < 10; ++i )
	{
		int q;
		EXPECT_TRUE( sut.receive( q ) );
		EXPECT_EQ( i, q );
	}

	EXPECT_TRUE( sut.empty() );
}

TEST( message_queue_test_suite, add_10_consume_all )
{
	auto sut = message_queue<int>{};

	for( auto i = 0u; i < 10; ++i )
		sut.send( i );

	std::vector<bool> a( 10, false );
	auto              r = sut.receive_all( [&]( int i )
        {
            a[i] = true;
            //! As this is sequentially run.. should be in order.
            if( i )
            {
                EXPECT_TRUE( a[i - 1] );
            }
        } );
	EXPECT_EQ( 10, r );

	EXPECT_TRUE( sut.empty() );
}

TEST( message_queue_test_suite, generate_10_consume_all )
{
	auto sut = message_queue<int>{};

	auto gen = [i = 0u]() mutable
	{ return i++; };
	sut.send( gen, 10 );

	std::vector<bool> a( 10, false );
	auto              r = sut.receive_all( [&]( int i )
        {
            a[i] = true;
            //! As this is sequentially run.. should be in order.
            if( i )
            {
                EXPECT_TRUE( a[i - 1] );
            }
        } );
	EXPECT_EQ( 10, r );

	EXPECT_TRUE( sut.empty() );
}

TEST( message_queue_test_suite, add_range_consume_all )
{
	auto sut = message_queue<int>{};

	std::vector<int> v = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	sut.send_range( v );

	std::vector<bool> a( 10, false );
	auto              r = sut.receive_all( [&]( int i )
        {
            a[i] = true;
            //! As this is sequentially run.. should be in order.
            if( i )
            {
                EXPECT_TRUE( a[i - 1] );
            }
        } );
	EXPECT_EQ( 10, r );

	EXPECT_TRUE( sut.empty() );
}

TEST( message_queue_test_suite, generate_move_only_10_consume_all )
{
	using move_only = std::unique_ptr<int>;
	auto sut = message_queue<move_only>{};

	auto gen = []() mutable
	{ return move_only{}; };
	sut.send( gen, 10 );

	auto counter = 0u;
	auto r = sut.receive_all( [&]( move_only&& ) { ++counter; } );
	EXPECT_EQ( 10, r );
	EXPECT_EQ( 10, counter );

	EXPECT_TRUE( sut.empty() );
}

TEST( message_queue_test_suite, clear )
{
	using move_only = std::unique_ptr<int>;
	auto sut = message_queue<move_only>{};

	auto gen = []() mutable { return move_only{}; };
	sut.send( gen, 10 );

	sut.clear();

	EXPECT_TRUE( sut.empty() );
}

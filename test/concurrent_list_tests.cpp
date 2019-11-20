//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include <vector>
#include <array>
#include <stk/container/experimental/concurrent_list.hpp>
#include <geometrix/utility/ignore_unused_warnings.hpp>

struct concurrent_list_test_fixture : ::testing::Test
{
	concurrent_list_test_fixture() = default;

	void SetUp(){}
	void TearDown(){}

	stk::thread::simple_qsbr qsbr;
};
TEST_F(concurrent_list_test_fixture, construct)
{
	using namespace stk;
	concurrent_list<int> v(qsbr);
	EXPECT_EQ(0, v.size());
}

TEST_F(concurrent_list_test_fixture, push_back)
{
	using namespace stk;
	concurrent_list<int> v(qsbr);
	v.push_back(10);
	EXPECT_EQ(1, v.size());
	EXPECT_EQ(10, v.front());
	EXPECT_EQ(10, v.back());
}

TEST_F(concurrent_list_test_fixture, push_back2)
{
	using namespace stk;
	concurrent_list<int> v(qsbr);
	v.push_back(10);
	v.push_back(20);
	EXPECT_EQ(2, v.size());
	EXPECT_EQ(10, v.front());
	EXPECT_EQ(20, v.back());
}

TEST_F(concurrent_list_test_fixture, push_front)
{
	using namespace stk;
	concurrent_list<int> v(qsbr);
	v.push_front(10);
	EXPECT_EQ(1, v.size());
	EXPECT_EQ(10, v.front());
	EXPECT_EQ(10, v.back());
}

TEST_F(concurrent_list_test_fixture, push_front2)
{
	using namespace stk;
	concurrent_list<int> v(qsbr);
	v.push_front(10);
	v.push_front(20);
	EXPECT_EQ(2, v.size());
	EXPECT_EQ(20, v.front());
	EXPECT_EQ(10, v.back());
}

TEST_F(concurrent_list_test_fixture, push_front3)
{
	using namespace stk;
	concurrent_list<int> v(qsbr);
	v.push_front(10);
	v.push_front(20);
	v.push_front(30);
	EXPECT_EQ(3, v.size());
	EXPECT_EQ(30, v.front());
	EXPECT_EQ(10, v.back());
}

TEST_F(concurrent_list_test_fixture, find)
{
	using namespace stk;
	using node = concurrent_list<int>::node;
	concurrent_list<int> v(qsbr);
	v.push_front(10);
	v.push_front(20);
	v.push_front(30);

	EXPECT_TRUE(v.find([](node* pNode) { return pNode->data == 10; }));
	EXPECT_TRUE(v.find([](node* pNode) { return pNode->data == 20; }));
	EXPECT_TRUE(v.find([](node* pNode) { return pNode->data == 30; }));
	EXPECT_FALSE(v.find([](node* pNode) { return pNode->data == 40; }));
}

TEST_F(concurrent_list_test_fixture, erase)
{
	using namespace stk;
	using node = concurrent_list<int>::node;
	concurrent_list<int> v(qsbr);
	v.push_front(10);
	auto n = v.push_front(20);
	v.push_front(30);

	v.erase(n);

	EXPECT_TRUE(v.find([](node* pNode) { return pNode->data == 10; }));
	EXPECT_FALSE(v.find([](node* pNode) { return pNode->data == 20; }));
	EXPECT_TRUE(v.find([](node* pNode) { return pNode->data == 30; }));
}

TEST_F(concurrent_list_test_fixture, erase_if)
{
	using namespace stk;
	concurrent_list<int> v(qsbr);
	using node = concurrent_list<int>::node;
	v.push_front(10);
	v.push_front(20);
	v.push_front(30);

	v.erase_if([](node* pNode) { return pNode->data == 20; });

	EXPECT_TRUE(v.find([](node* pNode) { return pNode->data == 10; }));
	EXPECT_FALSE(v.find([](node* pNode) { return pNode->data == 20; }));
	EXPECT_TRUE(v.find([](node* pNode) { return pNode->data == 30; }));
}

struct a_type
{
	a_type() = default;

	a_type(int* i)
		: i(i)
	{
		++(*i);
	}

	~a_type()
	{
		if (i)--(*i);
	}
	
	int* i{ nullptr };
};

TEST_F(concurrent_list_test_fixture, allocator_test)
{
	using namespace stk;
	int count = 0;
	concurrent_list<a_type> v(qsbr);
	auto v1 = v.emplace_front(&count);
	auto v2 = v.emplace_front(&count);
	auto v3 = v.emplace_front(&count);

	EXPECT_EQ(3, count);
	v.erase(v1);
	v.erase(v2);
	v.erase(v3);
	qsbr.release();
	EXPECT_EQ(0, count);
}

TEST_F(concurrent_list_test_fixture, pop_front)
{
	using namespace stk;
	int count = 0;
	concurrent_list<a_type> v(qsbr);
	v.emplace_front(&count);
	v.emplace_front(&count);
	v.emplace_front(&count);

	EXPECT_EQ(3, count);
	v.pop_front();
	v.pop_front();
	v.pop_front();
	qsbr.release();
	EXPECT_EQ(0, count);
}

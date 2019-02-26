
#include "pimpl_test.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

TEST(pimpl_test_suite, default_construct)
{
	A a;
}

TEST(pimpl_test_suite, unary_construct)
{
	A a(10);
	EXPECT_EQ(10, a.get_x());
}

TEST(pimpl_test_suite, copy)
{
	A a(10);
	A b = a;
	EXPECT_EQ(10, a.get_x());
	EXPECT_EQ(100, b.get_x());
}

TEST(pimpl_test_suite, move)
{
	A a(10);
	A b = std::move(a);
	EXPECT_FALSE(a.is_valid());
	EXPECT_EQ(10, b.get_x());
}

TEST(pimpl_test_suite, default_construct_no_copy_no_move)
{
	A_no_copy_no_move a;
}

TEST(pimpl_test_suite, unary_construct_no_copy_no_move)
{
	A_no_copy_no_move a(10);
	EXPECT_EQ(10, a.get_x());
}

TEST(pimpl_test_suite, default_construct_no_copy)
{
	A_no_copy a;
}

TEST(pimpl_test_suite, unary_construct_no_copy)
{
	A_no_copy a(10);
	EXPECT_EQ(10, a.get_x());
}

TEST(pimpl_test_suite, move_no_copy)
{
	A_no_copy a(10);
	A_no_copy b = std::move(a);
	EXPECT_FALSE(a.is_valid());
	EXPECT_EQ(10, b.get_x());
}

TEST(pimpl_test_suite, virtual_pimpl_is_destructed)
{
	bool deleted = false;
	{
		B b(deleted);
	}
	EXPECT_TRUE(deleted);
}

TEST(pimpl_test_suite, lamdba_deleter)
{
	struct my_type 
	{
		my_type(bool& d)
			: d(d)
		{}

		~my_type()
		{
			d = true;
		}

		bool& d;
	};

	bool deleted = false;
	{
		auto p = stk::pimpl<my_type>(new my_type(deleted), [](my_type* p) { delete p; });
	}

	EXPECT_TRUE(deleted);
}

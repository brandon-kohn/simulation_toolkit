
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

TEST(pimpl_test_suite, default_construct_no_copy_no_move)
{
	A_no_copy_no_move a;
}

TEST(pimpl_test_suite, unary_construct_no_copy_no_move)
{
	auto a = A_no_copy_no_move(10);
	EXPECT_EQ(10, a.get_x());
}

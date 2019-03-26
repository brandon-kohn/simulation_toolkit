#include <gtest/gtest.h>
#include <boost/config.hpp>

TEST(test_shared1_suite, test1)
{
    EXPECT_TRUE(true);
}

BOOST_SYMBOL_EXPORT bool pull_in_shared1(){ return true; }

extern "C" BOOST_SYMBOL_EXPORT int MODULE_1_TESTS(int* argc, char** argv)
{
	testing::InitGoogleTest(argc, argv);
	auto r = RUN_ALL_TESTS();
	return r;
}

#include <gtest/gtest.h>
#include <boost/config.hpp>

TEST(test_shared2_suite, test2)
{
    EXPECT_TRUE(true);
}

BOOST_SYMBOL_IMPORT bool pull_in_shared1();

extern "C" BOOST_SYMBOL_EXPORT int RUN_GOOGLE_TESTS(int* argc, char** argv)
{
    auto b = pull_in_shared1();
	testing::InitGoogleTest(argc, argv);
	auto r = RUN_ALL_TESTS();
	return r;
}

#include <gtest/gtest.h>
#include <boost/config.hpp>

TEST(test_shared2_suite, test2)
{
    EXPECT_TRUE(true);
}

extern "C" BOOST_SYMBOL_EXPORT int RUN_GOOGLE_TESTS(int* argc, char** argv)
{
	testing::InitGoogleTest(argc, argv);
	auto r = RUN_ALL_TESTS();
	return r;
}

cmake_minimum_required(VERSION 3.0.2)
 
# Now simply link your own targets against gtest, gmock,
# etc. as appropriate
add_executable(rtree_cache_tests rtree_cache_tests.cpp)
target_link_libraries(rtree_cache_tests gtest gmock_main)
add_test(NAME rtree_cache_tests COMMAND rtree_cache_tests)
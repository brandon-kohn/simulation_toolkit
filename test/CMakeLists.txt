cmake_minimum_required(VERSION 3.0.2)
 
find_package(Boost 1.60.0)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  message("Boost found: ${Boost_INCLUDE_DIRS}")
  add_definitions(-DBOOST_RESULT_OF_USE_TR1_WITH_DECLTYPE_FALLBACK -DBOOST_CHRONO_HEADER_ONLY -D"BOOST_PARAMETER_MAX_ARITY=20")
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# link targets against gtest, gmock
#if(Boost_FOUND)
#    add_executable(rtree_cache_tests rtree_cache_tests.cpp)
#    target_link_libraries(rtree_cache_tests gtest gmock_main clipper-lib stk geometrix poly2tri)
#    add_test(NAME rtree_cache_tests COMMAND rtree_cache_tests)
#endif()

if(Boost_FOUND)
    set(tests
        rtree_cache_tests
        temporary_vertex_graph_adaptor_tests
        stoppable_astar_tests
       )

    foreach(test ${tests})
        # add example executable
        add_executable(${test} ${test}.cpp)
        target_link_libraries(${test} gtest gmock_main clipper-lib stk geometrix poly2tri)
        add_test(NAME ${test} COMMAND ${test})
    endforeach()
endif()
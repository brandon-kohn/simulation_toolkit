cmake_minimum_required(VERSION 3.0.2)
 
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.60.0 REQUIRED COMPONENTS thread system context fiber)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  link_directories(${Boost_LIBRARY_DIRS})
  add_definitions(-DBOOST_RESULT_OF_USE_TR1_WITH_DECLTYPE_FALLBACK -DBOOST_CHRONO_HEADER_ONLY -D"BOOST_PARAMETER_MAX_ARITY=20" -D"BOOST_THREAD_VERSION=4" -DBOOST_ALL_NO_LIB)
  message("Boost found: ${Boost_INCLUDE_DIRS}")  
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(Boost_FOUND)
    set(tests
        rtree_cache_tests
        temporary_vertex_graph_adaptor_tests
        stoppable_astar_tests
        stoppable_bfs_tests
        transformer_tests
        clipper_tests
        concurrency_tests
       )

    foreach(test ${tests})
        add_executable(${test} ${test}.cpp)
        target_link_libraries(${test} gtest gmock_main clipper-lib stk geometrix poly2tri ${Boost_LIBRARIES})
        add_test(NAME ${test} COMMAND ${test})
        set_property(TEST ${test} PROPERTY ENVIRONMENT "PATH=${Boost_LIBRARY_DIRS};$ENV{PATH}" )
    endforeach()

    add_executable(fiber_timing_tests fiber_timing_tests.cpp)
    target_link_libraries(fiber_timing_tests gtest gmock_main stk geometrix turf junction ${Boost_LIBRARIES})
    add_executable(skynet_tests skynet_tests.cpp pool_work_stealing.cpp)
    target_link_libraries(skynet_tests gtest gmock_main stk geometrix ${Boost_LIBRARIES})

endif()

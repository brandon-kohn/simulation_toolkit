cmake_minimum_required(VERSION 3.0.2)
 
find_package(Boost 1.60.0 COMPONENTS thread system)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  message("Boost found: ${Boost_INCLUDE_DIRS}")  
  add_definitions(-DBOOST_RESULT_OF_USE_TR1_WITH_DECLTYPE_FALLBACK -DBOOST_CHRONO_HEADER_ONLY -D"BOOST_PARAMETER_MAX_ARITY=20")
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
       )

    foreach(test ${tests})
        add_executable(${test} ${test}.cpp)
        target_link_libraries(${test} gtest gmock_main clipper-lib stk geometrix poly2tri ${Boost_LIBRARIES})
        add_test(NAME ${test} COMMAND ${test})
    endforeach()
endif()

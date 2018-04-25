cmake_minimum_required(VERSION 3.1.0)
 
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
set(CMAKE_CXX_EXTENSIONS ON)

if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS -fext-numeric-literals)
endif()

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
        target_compile_definitions(${test} PRIVATE -DPOLY2TRI_STATIC_LIB -DCLIPPER_STATIC_LIB)
        target_link_libraries(${test} gtest_static gmock_main_static clipper_static stk geometrix poly2tri_static cds-s ${Boost_LIBRARIES})
        add_test(NAME ${test} COMMAND ${test})
        set_property(TEST ${test} PROPERTY ENVIRONMENT "PATH=${Boost_LIBRARY_DIRS};$ENV{PATH}" )
    endforeach()

    set(parallel_tests
        concurrency_tests
        concurrent_skip_list_tests
        hash_grid_tests
        thread_pool_tests
        rpmalloc_tests
        concurrent_vector_tests
        )

    foreach(test ${parallel_tests})
        add_executable(${test} ${test}.cpp)
        target_compile_definitions(${test} PRIVATE -DJUNCTION_STATIC_LIB -DTURF_STATIC_LIB -DCDS_USE_STATIC_LIB -DRPMALLOC_STATIC_LIB -DPOLY2TRI_STATIC_LIB -DCLIPPER_STATIC_LIB)
        target_link_libraries(${test} gtest_static gmock_main_static stk geometrix turf_static junction_static cds-s clipper_static poly2tri_static rpmalloc_static ${Boost_LIBRARIES})
        add_test(NAME ${test} COMMAND ${test})
        set_property(TEST ${test} PROPERTY ENVIRONMENT "PATH=${Boost_LIBRARY_DIRS};$ENV{PATH}" )
    endforeach()

endif()

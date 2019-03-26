cmake_minimum_required(VERSION 3.1.0)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.61.0 COMPONENTS thread system)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  link_directories(${Boost_LIBRARY_DIRS})
  add_definitions(-DBOOST_RESULT_OF_USE_TR1_WITH_DECLTYPE_FALLBACK -DBOOST_CHRONO_HEADER_ONLY -D"BOOST_PARAMETER_MAX_ARITY=20" -D"BOOST_THREAD_VERSION=4" -DBOOST_ALL_NO_LIB)
  if(NOT ${STK_HAS_THREAD_LOCAL})
      add_definitions(-DSTK_NO_CXX11_THREAD_LOCAL)
  endif()
  message("Boost found: ${Boost_INCLUDE_DIRS}")
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS -fext-numeric-literals)
    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    find_package(Threads)
endif()

if(Boost_FOUND)

    # pimpl test
    add_executable(pimpl_tests pimpl_test.cpp pimpl_tests.cpp)
    target_link_libraries(pimpl_tests gtest_static gmock_main_static stk geometrix exact_static ${Boost_LIBRARIES})
    add_test(NAME pimpl_tests COMMAND pimpl_tests)
    set_property(TEST pimpl_tests PROPERTY ENVIRONMENT "PATH=${Boost_LIBRARY_DIRS};$ENV{PATH}" )

    set(tests
        exact_tests
        random_tests
        biased_position_generator_tests
        weighted_mesh_tests
        vector_compare_tests
        fixed_point_tests
        rtree_cache_tests
        temporary_vertex_graph_adaptor_tests
        stoppable_astar_tests
        stoppable_bfs_tests
        transformer_tests
        clipper_tests
        timing_tests
        color_tests
       )

    foreach(test ${tests})
        add_executable(${test} ${test}.cpp)
        target_compile_definitions(${test} PRIVATE -DPOLY2TRI_STATIC_LIB -DCLIPPER_STATIC_LIB)
        target_link_libraries(${test} gtest_static gmock_main_static exact_static clipper_static stk geometrix poly2tri_static ${Boost_LIBRARIES})
        add_test(NAME ${test} COMMAND ${test})
        set_property(TEST ${test} PROPERTY ENVIRONMENT "PATH=${Boost_LIBRARY_DIRS};$ENV{PATH}" )
    endforeach()

    # Concurrency tests.
    if(Boost_THREAD_FOUND AND Boost_SYSTEM_FOUND)
        set(concurrency_test_suite
            static_thread_pool_exit_tests
            type_switch_tests
            thread_specific_tests
            concurrency_tests
            concurrent_integral_map_tests
            thread_pool_tests
            concurrent_skip_list_tests
            hash_grid_tests
            concurrent_vector_tests
            concurrent_list_tests
            rpmalloc_tests
            )

        foreach(test ${concurrency_test_suite})
            add_executable(${test} ${test}.cpp)
            target_compile_definitions(${test} PRIVATE -DJUNCTION_STATIC_LIB -DTURF_STATIC_LIB -DRPMALLOC_STATIC_LIB -DPOLY2TRI_STATIC_LIB -DCLIPPER_STATIC_LIB)
            target_link_libraries(${test} gtest_static gmock_main_static stk geometrix exact_static turf_static junction_static clipper_static poly2tri_static rpmalloc_static ${Boost_LIBRARIES})
            add_test(NAME ${test} COMMAND ${test})
            set_property(TEST ${test} PROPERTY ENVIRONMENT "PATH=${Boost_LIBRARY_DIRS};$ENV{PATH}" )
        endforeach()
    endif()

    # Google Test using modules and a test runner.
    find_package(Boost 1.61.0 COMPONENTS thread system filesystem)
    if(Boost_THREAD_FOUND AND Boost_SYSTEM_FOUND AND Boost_FILESYSTEM_FOUND)
        add_executable(local_gtest_runner ${CMAKE_SOURCE_DIR}/gtest_runner/gtest_runner.cpp)
        target_link_libraries(local_gtest_runner ${CMAKE_DL_LIBS} ${Boost_LIBRARIES})
        if(Threads_FOUND)
            target_link_libraries(local_gtest_runner Threads::Threads)
        endif()
        add_library(shared_module1 SHARED shared_module1.cpp)
        target_link_libraries(shared_module1 gtest ${Boost_LIBRARIES})
        #if(NOT MSVC)
            add_test(NAME shared_module1_test COMMAND local_gtest_runner shared_module1 -fname=MODULE_1_TESTS)
            set_tests_properties( shared_module1_test PROPERTIES ENVIRONMENT "PATH=${CMAKE_CURRENT_BINARY_DIR};$ENV{PATH}" )
        #endif()

        add_library(shared_module2 SHARED shared_module2.cpp)
        target_link_libraries(shared_module2 gtest shared_module1 ${Boost_LIBRARIES})

        #if(NOT MSVC)
            add_test(NAME shared_module2_test COMMAND local_gtest_runner shared_module2)
            set_tests_properties( shared_module2_test PROPERTIES ENVIRONMENT "PATH=${CMAKE_CURRENT_BINARY_DIR};$ENV{PATH}" )
        #endif()
    endif()
endif()

cmake_minimum_required(VERSION 3.1.0)
include(cmake/compile_check.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
if(APPLE)
    set(CMAKE_MACOSX_RPATH TRUE)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
 
# Prevent GoogleTest from overriding our compiler/linker options
# when building with Visual Studio
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_EXPLICIT_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "")
set(Boost_NO_BOOST_CMAKE ON CACHE BOOL "" FORCE) 
if(NOT "${FORCE_MSVC_RUNTIME}" STREQUAL "")
    message(STATUS "Forcing msvc runtime to ${FORCE_MSVC_RUNTIME}")
    include(cmake/stk_force_msvc_runtime.cmake)
    force_msvc_runtime()
endif()
if(NOT "${FORCE_MSVC_DEBUG_DEFINE}" STREQUAL "")
    message(STATUS "Forcing msvc debug define to ${FORCE_MSVC_DEBUG_DEFINE}")
    include(cmake/stk_force_msvc_runtime.cmake)
    force_msvc_debug_define()
endif()

if(BUILD_TESTS)
    # Add googletest directly to our build. This adds
    # the following targets: gtest, gtest_main, gmock
    # and gmock_main
    # Download and unpack googletest at configure time
    configure_file(GoogleTest.CMakeLists.txt.in googletest-download/CMakeLists.txt)
    execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" . WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )
    execute_process(COMMAND ${CMAKE_COMMAND} --build . WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST ON CACHE BOOL "" FORCE) 
    set(INSTALL_GMOCK ON CACHE BOOL "" FORCE) 
    add_subdirectory(${CMAKE_BINARY_DIR}/googletest-src
                     ${CMAKE_BINARY_DIR}/googletest-build)
endif()

# Build Properties
set(CMAKE_INSTALL_INCLUDEDIR "include" CACHE PATH "Default relative include path.")
set(CMAKE_INSTALL_LIBDIR "lib" CACHE PATH "Default relative lib path.")

# Targets
project(simulation_suite)

if(MSVC)
    CompileCheck(STK_HAS_CONSTEXPR "Checking for constexpr keyword" "static constexpr const char* foo = \"foo\";")
    CompileCheck(STK_HAS_THREAD_LOCAL "Checking for thread_local keyword" "static thread_local const char* foo = \"foo\";")
    CompileCheck(STK_HAS_CXX17_STD_ALIGNED_ALLOC "Checking for std::aligned_alloc(size, align)" "#include <cstdlib>
                                                                                                 void* foo = std::aligned_alloc(8, 32); 
                                                                                                 std::free(foo);")
else()
    CompileCheckWithFlags(STK_HAS_CONSTEXPR "Checking for constexpr keyword" "-std=c++11" "static constexpr const char* foo = \"foo\";")
    CompileCheckWithFlags(STK_HAS_THREAD_LOCAL "Checking for thread_local keyword" "-std=c++11" "static thread_local const char* foo = \"foo\";")
    CompileCheckWithFlags(STK_HAS_CXX17_STD_ALIGNED_ALLOC "Checking for std::aligned_alloc(size, align)" "-std=c++17" "#include <cstdlib>
                                                                                                                       void* foo = std::aligned_alloc(8, 32);
                                                                                                                       std::free(foo);")
endif()

if(${STK_HAS_CONSTEXPR})
    set(BUILD_LIBCDS ON CACHE INTERNAL "")
endif()

add_subdirectory(geometrix)
add_subdirectory(stk)
add_subdirectory(ska)
add_subdirectory(exact)
add_subdirectory(clipper-lib)
add_subdirectory(nlopt)
add_subdirectory(poly2tri)
add_subdirectory(junction)
add_subdirectory(entt)
add_subdirectory(doctest)
if(BUILD_LIBCDS)
    message(STATUS "Building LibCDS...")
    add_subdirectory(libcds)
endif()
if(NOT (CMAKE_COMPILER_IS_GNUCXX AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "5.0.0"))
    add_subdirectory(rpmalloc)
endif()

# Tests
if(BUILD_TESTS)
    add_subdirectory(gtest_runner)
    include(CTest)
    add_subdirectory(test)
endif()

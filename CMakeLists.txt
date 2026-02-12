cmake_minimum_required(VERSION 3.5.0)
include(cmake/compile_check.cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
if(APPLE)
    set(CMAKE_MACOSX_RPATH TRUE)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set_source_files_properties(stk/utility/synthetic_work.hpp PROPERTIES COMPILE_FLAGS "/FAcs")

 
# Prevent GoogleTest from overriding our compiler/linker options
# when building with Visual Studio
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_EXPLICIT_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_TESTS ON CACHE BOOL "")
#set(Boost_NO_BOOST_CMAKE OFF CACHE BOOL "" FORCE) 
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

# Build Properties
set(CMAKE_INSTALL_INCLUDEDIR "include" CACHE PATH "Default relative include path.")
set(CMAKE_INSTALL_LIBDIR "lib" CACHE PATH "Default relative lib path.")

# Targets
project(simulation_suite)

if(BUILD_TESTS)
    find_package(GTest REQUIRED)
endif()

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

add_subdirectory(geometrix)
add_subdirectory(stk)
add_subdirectory(ska)
add_subdirectory(exact)
add_subdirectory(clipper-lib)
add_subdirectory(poly2tri)
add_subdirectory(junction)
add_subdirectory(doctest)

# Tests
if(BUILD_TESTS)
    add_subdirectory(gtest_runner)
    include(CTest)
    add_subdirectory(test)
endif()

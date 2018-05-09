cmake_minimum_required(VERSION 3.1.0)
include(cmake/compile_check.cmake)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Download and unpack googletest at configure time
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
configure_file(GoogleTest.CMakeLists.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" . WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )
execute_process(COMMAND ${CMAKE_COMMAND} --build . WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )
 
# Prevent GoogleTest from overriding our compiler/linker options
# when building with Visual Studio
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_EXPLICIT_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "")

if(NOT "${FORCE_MSVC_RUNTIME}" STREQUAL "")
    message(STATUS "Forcing msvc runtime to ${FORCE_MSVC_RUNTIME}")
    include(cmake/stk_force_msvc_runtime.cmake)
    force_msvc_runtime()
endif()
 
# Add googletest directly to our build. This adds
# the following targets: gtest, gtest_main, gmock
# and gmock_main
add_subdirectory(${CMAKE_BINARY_DIR}/googletest-src
                 ${CMAKE_BINARY_DIR}/googletest-build)

# Build Properties
set(CMAKE_INSTALL_INCLUDEDIR "include" CACHE PATH "Default relative include path.")
set(CMAKE_INSTALL_LIBDIR "lib" CACHE PATH "Default relative lib path.")

# Targets
project(simulation_suite)

if(MSVC)
    CompileCheck(STK_HAS_CONSTEXPR "Checking for constexpr keyword" "static constexpr const char* foo = \"foo\";")
else()
    CompileCheckWithFlags(STK_HAS_CONSTEXPR "Checking for constexpr keyword" "-std=c++11" "static constexpr const char* foo = \"foo\";")
endif()

if(${STK_HAS_CONSTEXPR})
set(BUILD_LIBCDS ON CACHE INTERNAL "")
endif()

add_subdirectory(stk/)
add_subdirectory(clipper-lib/)
add_subdirectory(geometrix/)
add_subdirectory(nlopt/)
add_subdirectory(poly2tri/)
add_subdirectory(junction/)
###add_subdirectory(gtest_runner/)
if(BUILD_LIBCDS)
    message(STATUS "Building LibCDS...")
    if("$ENV{BOOST_ROOT}" STREQUAL "")
        message(FATAL_ERROR "BOOST_ROOT must be defined. NOTE: If using 'sudo' remember to invoke with 'sudo -E' to preserve the environment.")
    endif()
   add_subdirectory(libcds/)
endif()
add_subdirectory(rpmalloc/)

# Tests
if(BUILD_TESTS)
    include(CTest)
    add_subdirectory(test)
endif()

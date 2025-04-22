cmake_minimum_required(VERSION 3.1.0)
 
set(Boost_USE_STATIC_LIBS ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS -fext-numeric-literals)
    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    find_package(Threads)
endif()

# Google Test using modules and a test runner.
find_package(Boost 1.61.0 COMPONENTS thread system filesystem)
if(Boost_THREAD_FOUND AND Boost_SYSTEM_FOUND AND Boost_FILESYSTEM_FOUND)
    message("Boost found: ${Boost_INCLUDE_DIRS}")  
    include_directories(${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARY_DIRS})
    add_definitions(-DBOOST_CHRONO_HEADER_ONLY -D"BOOST_PARAMETER_MAX_ARITY=20" -D"BOOST_THREAD_VERSION=4" -DBOOST_ALL_NO_LIB)

    add_executable(gtest_runner gtest_runner.cpp)
    target_link_libraries(gtest_runner ${CMAKE_DL_LIBS} ${Boost_LIBRARIES})
    set_target_properties(gtest_runner PROPERTIES DEBUG_POSTFIX "d")
    if(Threads_FOUND)
        target_link_libraries(gtest_runner Threads::Threads)
    endif()
    # Deployment
    install (TARGETS gtest_runner
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
endif()

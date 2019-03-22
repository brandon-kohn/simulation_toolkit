cmake_minimum_required(VERSION 3.0.2)

# Build Properties
set(CMAKE_INSTALL_INCLUDEDIR "include" CACHE PATH "Default relative include path.")
set(CMAKE_INSTALL_LIBDIR "lib" CACHE PATH "Default relative lib path.")

if(MSVC AND NOT "${FORCE_MSVC_RUNTIME}" STREQUAL "")
    message(STATUS "exact: Forcing msvc runtime to ${FORCE_MSVC_RUNTIME}")
    force_msvc_runtime()
endif()

# Targets
# NOTE: Only static libs supported as STL are part of the interface.
project(exact)
set(HEADERS exact/predicates.hpp)
set(SOURCES exact/predicates.cpp)

set(EXACT_STATIC_LIBRARY ${PROJECT_NAME}_static)
set(EXACT_SHARED_LIBRARY ${PROJECT_NAME})

add_library(${EXACT_SHARED_LIBRARY} SHARED ${SOURCES} ${HEADERS})
set_target_properties(${EXACT_SHARED_LIBRARY} PROPERTIES DEBUG_POSTFIX "d")
set_target_properties (${EXACT_SHARED_LIBRARY} PROPERTIES VERSION ${EXACT_VERSION_STRING})
target_compile_definitions(${EXACT_SHARED_LIBRARY} PUBLIC -DEXACT_EXPORTS_API)
target_include_directories(${EXACT_SHARED_LIBRARY} INTERFACE $<BUILD_INTERFACE:${exact_SOURCE_DIR}> $<INSTALL_INTERFACE:include>)

add_library(${EXACT_STATIC_LIBRARY} STATIC ${SOURCES} ${HEADERS})
set_target_properties(${EXACT_STATIC_LIBRARY} PROPERTIES DEBUG_POSTFIX "d")
set_target_properties (${EXACT_STATIC_LIBRARY} PROPERTIES VERSION ${EXACT_VERSION_STRING})
target_include_directories(${EXACT_STATIC_LIBRARY} INTERFACE $<BUILD_INTERFACE:${exact_SOURCE_DIR}> $<INSTALL_INTERFACE:include>)
target_compile_definitions(${EXACT_STATIC_LIBRARY} PUBLIC -DEXACT_STATIC_LIB)

include_directories(exact)
           
# use, i.e. don't skip the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE) 

SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# the RPATH to be used when installing, but only if it's not a system directory
LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
IF("${isSystemDir}" STREQUAL "-1")
   SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
ENDIF("${isSystemDir}" STREQUAL "-1")

# Deployment
install ( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/exact/
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/exact
          FILES_MATCHING PATTERN "*.hpp")
install (TARGETS ${EXACT_STATIC_LIBRARY} ${EXACT_SHARED_LIBRARY}
         RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
         ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
         LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")

cmake_minimum_required(VERSION 3.5.0)

# Build Properties
set(CMAKE_INSTALL_INCLUDEDIR "include" CACHE PATH "Default relative include path.")

# Targets
project(stk)

add_library(stk INTERFACE)
target_include_directories(stk INTERFACE
  $<BUILD_INTERFACE:${stk_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
)

# Deployment
install ( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/stk/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/stk)
cmake_minimum_required(VERSION 3.0.2)

# Build Properties
set(CMAKE_INSTALL_INCLUDEDIR "include" CACHE PATH "Default relative include path.")

# Targets
project(ska)

add_library(ska INTERFACE)
target_include_directories(ska INTERFACE
  $<BUILD_INTERFACE:${ska_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
)

# Deployment
install ( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ska/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ska)

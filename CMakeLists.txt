cmake_minimum_required(VERSION 3.0.2)

# Build Properties
set(CMAKE_INSTALL_INCLUDEDIR "include" CACHE PATH "Default relative include path.")
set(CMAKE_INSTALL_LIBDIR "lib" CACHE PATH "Default relative lib path.")

# Targets
project(simulation_suite)

add_subdirectory(clipper-lib/)
add_subdirectory(geometrix/)
add_subdirectory(nlopt/)
add_subdirectory(poly2tri/)

# Deployment
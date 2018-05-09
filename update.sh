#!/bin/sh
# `simulation_suite` update and build script.
# This script does a pull on simulation_suite at the URL below.
# It will then invoke cmake to update and build the suite installing into the specified path.
# Requires git and cmake on the path.
# Note: Also requires the environment is configured for the specified CMAKE generator.

error_exit()
{
    echo "$1" 1>&2
    exit 1
}

if [ "$#" -lt 2 ]
  then
    error_exit "Usage: ./update.sh <Install Path Prefix> <\"CMake Generator\">"
fi

FORCE_MSVC_RUNTIME=""
if [ "$3" -ne "" ]
  then
    FORCE_MSVC_RUNTIME=$3
fi

# Update the repo
REPOSRC=https://github.com/brandon-kohn/simulation_suite.git

git pull "$REPOSRC" || error_exit "Git pull failed. Aborting."

git submodule update --init --recursive || error_exit "Git submodule update failed. Aborting."

INSTALL_PATH=$1
CMAKE_GENERATOR=$2

# Build the release
cmake -H. -Bcmake.build.release -DCMAKE_BUILD_TYPE=release -DCMAKE_INSTALL_PREFIX="$INSTALL_PATH" -DFORCE_MSVC_RUNTIME=$FORCE_MSVC_RUNTIME -G"$CMAKE_GENERATOR" || error_exit "CMake Release Build failed. Aborting."

# Install it
cmake --build "cmake.build.release" --config release --target install || error_exit "CMake Install failled. Aborting"

# Build the debug
cmake -H. -Bcmake.build.debug -DCMAKE_BUILD_TYPE=debug -DCMAKE_INSTALL_PREFIX="$INSTALL_PATH" -DFORCE_MSVC_RUNTIME=$FORCE_MSVC_RUNTIME -G"$CMAKE_GENERATOR" || error_exit "CMake Debug Build failled. Aborting"

# Install it
cmake --build "cmake.build.debug" --config debug --target install || error_exit "CMake Install failled. Aborting"

exit 0
# End

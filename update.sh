#!/bin/sh
# `simulation_suite` update and build script.
# This script does a pull on simulation_suite at the URL below.
# It will then invoke cmake to update and build the suite installing into the specified path.
# Requires git and cmake on the path.
# Note: Also requires the environment is configured for the specified CMAKE generator.

if [ "$#" -ne 2 ]
  then
    echo "Usage: ./update.sh <Install Path Prefix> <\"CMake Generator\">"
    exit 1
fi

# Update the repo
REPOSRC=https://github.com/brandon-kohn/simulation_suite.git

git pull "$REPOSRC"
ret=$?; if [[ $ret != 0 ]]; then exit $ret; fi

git submodule update --init --recursive
ret=$?; if [[ $ret != 0 ]]; then exit $ret; fi

INSTALL_PATH=$1
CMAKE_GENERATOR=$2

# Build the release
cmake -H. -Bcmake.build -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX="$INSTALL_PATH" -G"$CMAKE_GENERATOR"
ret=$?; if [[ $ret != 0 ]]; then exit $ret; fi

# Install it
cmake --build "cmake.build" --target install
ret=$?; if [[ $ret != 0 ]]; then exit $ret; fi

# Build the debug
cmake -H. -Bcmake.build -DCMAKE_BUILD_TYPE=DEBUG -DCMAKE_INSTALL_PREFIX="$INSTALL_PATH" -G"$CMAKE_GENERATOR"
ret=$?; if [[ $ret != 0 ]]; then exit $ret; fi

# Install it
cmake --build "cmake.build" --target install
ret=$?; if [[ $ret != 0 ]]; then exit $ret; fi

exit 0
# End
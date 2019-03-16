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
    error_exit "Usage: ./run_tests.sh <Config> <\"CMake Generator\">"
fi

# Update the repo
REPOSRC=https://github.com/brandon-kohn/simulation_suite.git

git pull "$REPOSRC" || error_exit "Git pull failed. Aborting."

git submodule update --init --recursive || error_exit "Git submodule update failed. Aborting."

CONFIG=$( echo "$1" | tr -s  '[:upper:]'  '[:lower:]' )
echo Running ${CONFIG} tests.
CMAKE_GENERATOR=$2

# Build the release
cmake -H. -B"cmake.build.${CONFIG}" -DBUILD_TESTS=1 -DCMAKE_BUILD_TYPE="${CONFIG}" -DCMAKE_INSTALL_PREFIX="cmake.install" -G"$CMAKE_GENERATOR" || error_exit "CMake Build failed. Aborting."

# Install it
cmake --build "cmake.build.${CONFIG}" --config "${CONFIG}" || error_exit "CMake Install failed. Aborting"

cd "cmake.build.${CONFIG}"
ctest -C --output-on-failure -V "${CONFIG}" $3 $4 $5 $6 $7 $8 $9 || error_exit "CTest failed to run tests."

exit 0
# End

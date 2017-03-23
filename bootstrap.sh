#!/bin/sh
# `simulation_suite` update and build script.
# This will pull down the suite from github if not already checked out.. or update it if is.
# It will then invoke cmake to update and build the suite installing into the specified path.
# Requires git and cmake on the path.

# Update the repo
REPOSRC=https://github.com/brandon-kohn/simulation_suite.git
LOCALREPO=simulation_suite

LOCALREPO_VC_DIR="$LOCALREPO"/.git

if [ ! -d "$LOCALREPO_VC_DIR" ]
then
    git clone "$REPOSRC" "$LOCALREPO"
    git submodule update --init --recursive
    cd "$LOCALREPO"
else
    cd "$LOCALREPO"
    git pull "$REPOSRC"
    git submodule update --init --recursive
fi

INSTALL_PATH=$1
CMAKE_GENERATOR=$2

# Build the release
cmake -H. -Bcmake.build -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX="$INSTALL_PATH" -G"$CMAKE_GENERATOR"
cmake --build "cmake.build" --target install

# Build the debug
cmake -H. -Bcmake.build -DCMAKE_BUILD_TYPE=DEBUG -DCMAKE_INSTALL_PREFIX="$INSTALL_PATH" -G"$CMAKE_GENERATOR"
cmake --build "cmake.build" --target install

# End
# Simulation ToolKit (STK)
[CMake][1] superproject for geometrix, clipper, poly2tri, nlopt and miscellaneous utilities for simulations.

Build instructions:

    git clone https://github.com/brandon-kohn/simulation_toolkit.git
    cd simulation_toolkit
    ./update.[sh|bat] <install path> <"CMake generator">

After running the above command the superproject and submodules will be built into the specified install path using "bin/include/lib/share" subdirectories.

To link with the libraries use:

    -lclipper-lib
    -lnlopt
    -lpoly2tri
    
To run stk tests:

    Boost version 1.60 or greater is required and must be specified via the environment before running the update.[sh/bat]. See [CMake.FindBoost][2] for details.
    
    e.g.:
    
    //! On Unix:
    export BOOST_ROOT=/path/to/boost_1.60
    ./update.sh "cmake.install" "Ninja"
    
    //! On Windows
    set BOOST_ROOT /path/to/boost_1.60
    ./update.bat "cmake.install" "NMake Makefiles"
    
    //! run the tests by:
    cd cmake.build
    ctest

[1]: https://cmake.org/
[2]: https://cmake.org/cmake/help/v3.0/module/FindBoost.html
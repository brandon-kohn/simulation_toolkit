# Simulation ToolKit (STK)
[CMake][1] superproject for geometrix, clipper, poly2tri, nlopt and miscellaneous utilities for simulations.

Libraries included: 

    clipper
    geometrix
    googletest
    nlopt
    poly2tri

Build instructions:

    git clone https://github.com/brandon-kohn/simulation_toolkit.git
    cd simulation_toolkit
    ./update.[sh|bat] <install path> <"CMake generator">

    //! NOTE: IDE generators such as Xcode will not work with the update script as the `cmake --build --config <Config>` is not respected. 
    //! Prefer makefile generators with the update script.
    
    //! NOTE: A third parameter may be specified to set the MSVC runtime flag : /MT[d] /MD[d]
    
After running the above command the superproject and submodules will be built into the specified install path using "bin/include/lib/share" subdirectories.

To link with the libraries use:

    -lclipper-lib
    -lnlopt
    -lpoly2tri
    -lgtest
    -lgmock
    
    //! NOTE: Append a 'd' to the library name for libraries compiled against the debug c-runtime.
    
To run stk tests (slow):

    Boost version 1.60 or greater is required and must be specified via the environment before running the update.[sh/bat]. See [CMake.FindBoost][2] for details.
    
    e.g.:
    
    //! On Unix:
    export BOOST_ROOT=/path/to/boost_1.60
    ./run_tests.sh debug "Ninja"
    
    //! On Windows
    set BOOST_ROOT /path/to/boost_1.60
    ./run_tests.bat release "NMake Makefiles"

[1]: https://cmake.org/
[2]: https://cmake.org/cmake/help/v3.0/module/FindBoost.html
# Simulation ToolKit (STK)
[CMake][1] superproject for cds, clipper, junction, geometrix, poly2tri, nlopt, turf and miscellaneous utilities for simulations.

##### Libraries included: 

    libcds
    clipper
    geometrix
    googletest
    junction
    nlopt
    poly2tri
    turf

##### Requirements:
###### Requires C++11 for boost.fiber, junction, and libcds.
###### Boost version 1.65.1 is required with the following compiled libraries:
        context
        fiber
        system
        thread

###### The boost libraries should be built with the following options:
    
    b2 --prefix=<install prefix> toolset=<toolset> address-model=64 link=static,shared threading=multi --layout=versioned --with-system --with-thread --with-fiber --with-context install
    
##### Build instructions:

    git clone https://github.com/brandon-kohn/simulation_toolkit.git
    cd simulation_toolkit
   
    Boost must be specified via the environment before running the update.[sh/bat]. See [CMake.FindBoost][2] for details.
    
    //! On Unix:
    export BOOST_ROOT=/path/to/boost
    ./update.sh <install path> <"CMake Generator">
    
    //! On Windows
    set BOOST_ROOT /path/to/boost
    .\update.bat <install path> <"CMake generator">


    //! NOTE: IDE generators such as Xcode will not work with the update script as the `cmake --build --config <Config>` is not respected. 
    //! Prefer makefile generators with the update script.
    
After running the above command the superproject and submodules will be built into the specified install path using "bin/include/lib/share" subdirectories.

##### To link with the libraries use:

    -lclipper-lib
    -lnlopt
    -lpoly2tri
    -lgtest
    -lgmock
    -lcds
    -ljunction
    -lturf
    //! NOTE: Append a 'd' to the library name for libraries compiled against the debug c-runtime. CDS uses '_d'.
    
##### To run stk tests (slow):

    Boost must be specified via the environment before running the run_tests.[sh/bat]. See [CMake.FindBoost][2] for details.
    
    e.g.:
    
    //! On Unix:
    export BOOST_ROOT=/path/to/boost
    ./run_tests.sh debug "Ninja"
    
    //! On Windows
    set BOOST_ROOT /path/to/boost
    ./run_tests.bat release "NMake Makefiles"

[1]: https://cmake.org/
[2]: https://cmake.org/cmake/help/v3.0/module/FindBoost.html

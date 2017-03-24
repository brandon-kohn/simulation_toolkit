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

[1]: https://cmake.org/

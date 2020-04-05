Build for Rez
-------------

Use the internal 'oiio-feedstock' repo and just do a `rez build` or `rez
release`, per the README instructions in that repository.



SpComp2 build and release
-------------------------

If you are trying to do a legacy SpComp2 release:

First, to make a TEST build and release to your local spcomp2 mock-up
in /net/soft_scratch/users/$USER, use this alternate target:

    make OIIO_SPCOMP2=1 spcomp2_install_local

When you are ready for the real thing, you will want to build each of the
following variants:

    # Python 2.7, Boost 1.55, C++11/gcc4.8 compat
    make nuke
    make OIIO_SPCOMP2=1 BOOSTVERS=1.55 OPENEXR_VERSION=2.2.0 SPI_COMPILER_PLATFORM=gcc-4.8 spcomp2_install

    # Python 2.7, Boost 1.70, C++11/gcc6.3 compat
    make nuke
    make OIIO_SPCOMP2=1 BOOSTVERS=1.70 PYTHON_VERSION=2.7 OPENEXR_VERSION=2.4.0 SPI_COMPILER_PLATFORM=gcc-6.3 spcomp2_install

Nobody should do this but lg, except in extraordinary circumstances.

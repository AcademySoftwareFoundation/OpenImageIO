Build for Rez/general use
-------------------------

Either locally (if you already have any dependencies in ext/* checked out),
or on compile42 (if it needs internet access to download dependency repos):

    cd site/spi/rez
    rez build -i

That locally installs to `/shots/spi/home/dev/$USER/software/packages`

If it looks good, then you can do this (must be on compile42):

    rez release --skip-repo-errors

(also from the rez subdirectory)

That command will release the dists to the studio.


Appwrapper binary releases
--------------------------

This step is for the ONE general/rez variant that we believe is the
canonical source of command line oiiotool and maketx. After building and
releasing all rez distros as above,

    cp build/release/gcc-6.3/python-2.7/boost-1.70/OpenImageIO_*.xml /shots/spi/home/lib/app_cfg/OpenImageIO

That will make appcfg aware of the release.

To also make this release the new facility default:

    db-any spi/home/OpenImageIO.bin highest /shots/spi/home/lib/app_cfg/OpenImageIO/OpenImageIO_A.B.C.D.xml

where A.B.C.D is the version.


SpComp2 build and release
-------------------------

If you are trying to do an SpComp2 release, forget all the steps above, you
will need some different flags.

First, to make a TEST build and release to your local spcomp2 mock-up
in /net/soft_scratch/users/$USER, use this alternate target:

    make OIIO_SPCOMP2=1 ... spcomp2_install_local

When you are ready for the real thing, you will want to build each of the
following variants:

    # Python 2.7, Boost 1.55, C++11/gcc4.8 compat
    make nuke
    make OIIO_SPCOMP2=1 BOOSTVERS=1.55 SPI_COMPILER_PLATFORM=gcc-4.8 spcomp2_install

    # Python 2.7, Boost 1.70, C++11/gcc6.3 compat
    make nuke
    make OIIO_SPCOMP2=1 BOOSTVERS=1.70 PYTHON_VERSION=2.7 OPENEXR_VERSION=2.4.0 SPI_COMPILER_PLATFORM=gcc-6.3 spcomp2_install

    # DOES ANYONE NEED THIS?
    # Python 2.7, Boost 1.55 sp namespaced, C++11/gcc4.8 compat
    # make nuke
    # make OIIO_SPCOMP2=1 BOOSTVERS=1.55 BOOSTSPSUFFIX=sp SPI_COMPILER_PLATFORM=gcc-4.8 spcomp2_install

    # DOES ANYONE NEED THIS?
    # Python 3.7, Boost 1.66, C++11/gcc6.3 compat
    # make nuke
    # make OIIO_SPCOMP2=1 BOOSTVERS=1.66 PYTHON_VERSION=3.7 SPI_COMPILER_PLATFORM=gcc-6.3 spcomp2_install

Nobody should do this but lg, except in extraordinary circumstances.

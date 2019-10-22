Build a variant for Rez/general use
-----------------------------------

Skip this section if you are instead building for SpComp2.

Note: For testing or to make your own local rez package, you can customize
either the rez package name, or the rez install location name, with make
flags `OIIO_REZ_NAME=blah REZ_PACKAGE_ROOT=/path/to/my/rez/pkgs` appended
to the `make` commands of any of the variants listed below. For example,

    make OIIO_SPIREZ=1 OIIO_REZ_NAME=oiio_test REZ_PACKAGE_ROOT=/path/to/my/rez/pkgs


Variants:

    # C++11/gcc4.8 compat, Python 2.7, Boost 1.55
    make nuke
    make spi OIIO_SPIREZ=1

    # C++11/gcc4.8 compat, Python 2.7, Boost 1.55 sp namespaced
    make nuke
    make spi OIIO_SPIREZ=1 BOOSTSPSUFFIX=sp

    # C++11/gcc6.3 compat, Python 3.7, Boost 1.66
    make nuke
    make spi OIIO_SPIREZ=1 BOOSTVERS=1.66 PYTHON_VERSION=3.7 SPI_COMPILER_PLATFORM=gcc-6.3 OpenColorIO_ROOT=/shots/spi/home/lib/SpComp2/OpenColorIO/rhel7-gcc48m64/v2

    # VFXPlatform 2020-ish: C++14/gcc6.3 compat, Python 3.7, Boost 1.70,
    # OpenEXR 2.4
    make nuke
    make spi OIIO_SPIREZ=1 BOOSTVERS=1.70 PYTHON_VERSION=3.7 SPI_COMPILER_PLATFORM=gcc-6.3 OpenColorIO_ROOT=/shots/spi/home/lib/SpComp2/OpenColorIO/rhel7-gcc48m64/v2 OPENEXR_VERSION=2.4.0 ENABLE_OPENVDB=0

    # C++11/gcc4.8 compat, Python 3.6, Boost 1.55 (for Jon Ware)
    make nuke
    make spi OIIO_SPIREZ=1 PYTHON_VERSION=3.6

You can do any of these on your local machine.


Rez/general release (do for each variant)
-----------------------------------------

This must be done from compile40 or compile42 (for correct write permissions
on certain shared directories), even if you did the build itself locally.

For any of the variants that you built above:

    ( cd dist/rhel7 ; rez release --skip-repo-errors )

That command will release the dist to the studio.


Appwrapper binary releases
--------------------------

This step is for the ONE general/rez variant that we believe is the
canonical source of command line oiiotool and maketx. After building and
releasing as above,

    cp dist/rhel7/OpenImageIO_*.xml /shots/spi/home/lib/app_cfg/OpenImageIO

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
    make OIIO_SPCOMP2=1 spcomp2_install

    # Python 2.7, Boost 1.55 sp namespaced, C++11/gcc4.8 compat
    make nuke
    make OIIO_SPCOMP2=1 BOOSTSPSUFFIX=sp spcomp2_install

    # Python 3.7, Boost 1.66, C++11/gcc6.3 compat
    make nuke
    make OIIO_SPCOMP2=1 BOOSTVERS=1.66 PYTHON_VERSION=3.7 SPCOMP2_COMPILER=gcc63 OpenColorIO_ROOT=/shots/spi/home/lib/SpComp2/OpenColorIO/rhel7-gcc48m64/v2 spcomp2_install

    # Python 3.6, Boost 1.55, C++11/gcc4.8 compat (for Jon Ware)
    make nuke
    make OIIO_SPCOMP2=1 PYTHON_VERSION=3.6 spcomp2_install

Nobody should do this but lg, except in extraordinary circumstances.

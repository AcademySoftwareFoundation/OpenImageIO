Installation instructions for OpenImageIO
=========================================

# Table of Contents
1. [OpenImageIO's Dependencies](#dependencies)
2. [Installing binaries from package managers](#installing-from-package-managers)
3. [Building OIIO from source](#building-from-source)


Dependencies
------------

NEW or CHANGED MINIMUM dependencies since the last major release are **bold**.

### Required dependencies -- OIIO will not build at all without these

 * C++14 or higher (also builds with C++17, and C++20)
     * The default build mode is C++14. This can be controlled by via the
       CMake configuration flag: `-DCMAKE_CXX_STANDARD=17`, etc.
 * Compilers: gcc 6.1 - 12.1, clang 3.4 - 16, MSVS 2017 - 2019,
   Intel icc 17+, Intel OneAPI C++ compiler 2022+.
 * **CMake >= 3.15** (tested through 3.27)
 * **OpenEXR/Imath >= 2.4** (recommended: 3.1 or higher; tested through 3.2
   and main) (ADVISORY: We expect that OIIO 2.6 in 2024 will require OpenEXR >= 3.1)
 * libTIFF >= 3.9 (recommended: 4.0+; tested through 4.5)
 * libjpeg >= 8, or libjpeg-turbo >= 1.1 (tested through jpeg9d and jpeg-turbo
   2.1)
 * Boost >= 1.53 (recommended: at least 1.66; tested through 1.81)
 * [fmtlib](https://github.com/fmtlib/fmt) >= 6.1.2 (tested through 10.0). If
   not found at build time, this will be automatically downloaded unless the
   build sets `-DBUILD_MISSING_FMT=OFF`.

### Optional dependencies -- features may be disabled if not found
 * If you are building the `iv` viewer (which will be disabled if any of
   these are not found):
     * Qt5 >= 5.6 (tested through 5.15) or Qt6 (tested through 6.4)
     * OpenGL
 * If you are building the Python bindings or running the testsuite:
     * Python >= 2.7 (tested against 2.7, 3.7, 3.8, 3.9, 3.10, 3.11)
     * pybind11 >= 2.4.2 (Tested through 2.11. Note that pybind11 v2.10+ does
       not support Python < 3.6.)
     * NumPy
 * If you want support for camera "RAW" formats:
     * LibRaw >= 0.18 (tested though 0.21.1; if
       building with C++17 or higher, LibRaw >= 0.20 is necessary)
 * If you want support for a wide variety of video formats:
     * ffmpeg >= 3.0 (tested through 6.0)
 * If you want support for jpeg 2000 images:
     * OpenJpeg >= 2.0 (tested through 2.5; we recommend 2.4 or higher
       for multithreading support)
 * If you want support for OpenVDB files:
     * OpenVDB >= 5.0 (tested through 10.0)
 * If you want to use TBB as the thread pool:
     * TBB >= 2018 (tested through 2021 and OneTBB)
 * If you want support for converting to and from OpenCV data structures,
   or for capturing images from a camera:
     * OpenCV 3.x, or 4.x (tested through 4.5)
 * If you want support for GIF images:
     * giflib >= 4.1 (tested through 5.2; 5.0+ is strongly recommended for
       stability and thread safety)
 * If you want support for HEIF/HEIC or AVIF images:
     * libheif >= 1.3 (1.7 required for AVIF support, tested through 1.16)
     * libheif must be built with an AV1 encoder/decoder for AVIF support.
     * Avoid libheif 1.10 on Mac, it is very broken. Libheif 1.11+ is fine.
 * If you want support for DICOM medical image files:
     * DCMTK >= 3.6.1 (tested through 3.6.7)
 * If you want support for WebP images:
     * WebP >= 0.6.1 (tested through 1.3.1)
 * If you want support for OpenColorIO color transformations:
     * OpenColorIO >= 1.1 (tested through 2.2; 2.0+ is recommended)
 * If you want support for Ptex:
     * Ptex >= 2.3.1 (probably works for older; tested through 2.4.2)
 * If you want to be able to do font rendering into images:
     * Freetype (minimum unknown, tested 2.8 through 2.13)
 * We use PugiXML for XML parsing. There is a version embedded in the OIIO
   tree, but if you want to use an external, system-installed version (as
   may be required by some software distributions with policies against
   embedding other projects), then just build with `-DUSE_EXTERNAL_PUGIXML=1`.
   Any PugiXML >= 1.8 should be fine (we have tested through 1.13).



Supported platforms at present include Linux (32 and 64 bit),
Mac OS X, and Windows.

Our build system is based upon 'CMake'.  If you don't already have it
installed on your system, you can get it from http://www.cmake.org

If certain dependencies (robin-map and fmtlib) are not found, their sources
will be retrieved and built into libraries, as part of the build process. The 
sources of those dependencies are cloned from their Git repo, hence `git` must 
be available as a command.

After you build OpenImageIO, if you compiled with the `EMBEDPLUGINS=0` flag
you will need to set the environment variable `OIIO_LIBRARY_PATH` to point
to the 'lib' directory where OpenImageIO is installed, or else it will
not be able to find the plugins.


Installing from package managers
================================

If all you want to do is install the OIIO libraries, headers, and command
line tools as quickly as possible (don't need OIIO source or any custom
build options), maybe one of these packages managers will do it for you:

* vcpkg (https://github.com/Microsoft/vcpkg)
    * https://github.com/Microsoft/vcpkg/tree/master/ports/openimageio
    * `vcpkg install openimageio [tools]`
    * For a full list of supported build features: `vcpkg search openimageio`
    * Instructions for building a Python wheel on Windows: 
      https://github.com/Correct-Syntax/py-oiio
* homebrew (https://github.com/Homebrew/brew)
    * https://formulae.brew.sh/formula/openimageio
    * `brew install openimageio`
* macports (https://github.com/macports/macports-ports)
    * https://www.macports.org/ports.php?by=name&substr=openimageio
    * `port install openimageio`
* fink (https://github.com/fink/fink)
    * http://pdb.finkproject.org/pdb/package.php/libopenimageio2.1-shlibs
    * `fink install openimageio`
* conan (https://github.com/conan-io/conan)

If these work for you and it's all you need, bingo! You are done.

You may find this guide to versions carried by distributions helpful:

[![OpenImageIO packaging status](https://repology.org/badge/vertical-allrepos/openimageio.svg?exclude_unsupported=1&columns=3&exclude_sources=modules,site&header=OpenImageIO%20packaging%20status)](https://repology.org/project/openimageio/versions)


Building from source
====================


Dependency control and disabling components
-------------------------------------------

**Hints for finding dependencies**

For each external dependency PkgName, our CMake build system will recognize
the following optional variable:

    PkgName_ROOT=...

to specify a hint about where the package is installed. It can either be
a CMake variable (set by `-DPkgName_ROOT=...` on the CMake command line),
or an environment variable of the same name, or a variable setting on the
Make wrapper (`make PkgName_ROOT=...`).

**Disabling optional dependencies and individual components**

`USE_PYTHON=0` : Omits building the Python bindings.

`OIIO_BUILD_TESTS=0` : Omits building tests (you probably don't need them
unless you are a developer of OIIO or want to verify that your build
passes all tests).

`OIIO_BUILD_TOOLS=0` : Disables building all the command line tools (such
as iinfo, oiiotool, maketx, etc.).

`ENABLE_toolname=0` : Disables building the named command line tool (iinfo,
oiiotool, etc.). This works both as a CMake variable and also as an
environment variable.

`ENABLE_formatname=0` : Disables building support for the particular named
file format (jpeg, fits, png, etc.). This works both as a CMake variable and
also as an environment variable.

`ENABLE_PkgName=0` : Disables use of an *optional* dependency (such as
FFmpeg, OpenVDB, Webp, etc.) -- even if the dependency is found on the
system. This will obviously disable any functionality that requires the
dependency. This works both as a CMake variable and
also as an environment variable.



Building OpenImageIO on Linux or OS X
-------------------------------------

The following dependencies must be installed to build the core of
OpenImageIO:
* Boost
* libjpeg
* libtiff
* libpng
* OpenEXR.

These can be installed using the standard package managers on your system.
Optionally, to build the image viewing tools, you will need Qt and OpenGL.

On OS X, these dependencies can be installed using Fink, MacPorts or
Homebrew.  After installation of any of these package installers, use
the "fink", "port" or "brew" commands (respectively) to install the
dependencies (e.g. "fink install libpng16", "port install qt4-mac" or "brew
update; brew doctor; brew install qt") before invoking make as described
below.

On OS X, Fink can also be used to directly compile and install the OpenImageIO
tools directly with the command "fink install openimageio-tools".  On OS X
releases where Fink has a binary distribution (10.8, 10.9, and 10.10 as
of 2015), the command "apt-get install openimageio-tools" will fetch
prebuilt binaries.

Dependent libraries can be installed in either the system default
locations or in custom directories.  Libraries installed in custom
directories must notify the CMake system using environment variables.
For example, set QTDIR to point at the root of the Qt library location
so that CMake can find it (see CMake configuration output).

**On Linux and OS X, you can build from source from the top-level
directory by just typing 'make'.  (Yes, we have a 'make' wrapper around
our CMake build, it simplifies things.)**

During the make, various temporary files (object files, etc.) will
be put in build/PLATFORM, where 'PLATFORM' will be the name of the
platform you are building for (e.g., linux, linux64, macosx).

The result of the make will be a full binary distribution in
dist/PLATFORM.

Make targets you should know about:

|  Target           |  Command                                           |
| ----------------- | -------------------------------------------------- |
|  make             |  Build an optimized distro in dist/PLATFORM, with temp files created while building in build/PLATFORM. |
|  make debug       |  Build a debugging (symbols, not stripped) distro, will end up in dist/PLATFORM.debug |
|  make clean       |  Get rid of all the temporary files in build/PLATFORM |
|  make realclean   |  Get rid of both build/PLATFORM and dist/PLATFORM     |
|  make nuke        |  Get rid of all build/ and dist/, for all platforms   |
|  make profile     |  Build a profilable version dist/PLATFORM.profile     |
|  make help        |  Print all the make options                           |

Additionally, a few helpful modifiers alter some build-time options:

|  Target                   |  Command                                       |
| ------------------------- | ---------------------------------------------- |
| make VERBOSE=1 ...        |  Show all compilation commands                 |
| make STOP_ON_WARNING=0    |  Do not stop building if compiler warns        |
| make EMBEDPLUGINS=0 ...   |  Don't compile the plugins into libOpenImageIO |
| make USE_OPENGL=0 ...     |  Skip anything that needs OpenGL               |
| make USE_QT=0 ...         |  Skip anything that needs Qt                   |
| make MYCC=xx MYCXX=yy ... |  Use custom compilers                          |
| make USE_PYTHON=0 ...     |  Don't build the Python binding                |
| make BUILD_SHARED_LIBS=0  |  Build static library instead of shared        |
| make LINKSTATIC=1 ...     |  Link with static external libraries when possible |
| make SOVERSION=nn ...     |  Include the specified major version number in the shared object metadata |
| make NAMESPACE=name       |   Wrap everything in another namespace         |

The command 'make help' will list all possible options.

You can also ignore the top level Makefile wrapper, and instead use
CMake directly:

    cmake -B build -S .
    cmake --build build --target install

If the compile stops because of warnings, try again with

    make nuke
    make STOP_ON_WARNING=0

or, if you are using CMake directly,

    rm -rf build
    cmake -B build -S . -DSTOP_ON_WARNING=0
    cmake --build build --target install




Building on Windows
-------------------

**Method 1 - from source**

You will need to have Git, CMake and Visual Studio installed.

The minimal set of dependencies for OIIO is: Boost, zlib, libTIFF, OpenEXR, and libjpeg or libjpeg-turbo. If you have them built somewhere then you skip
the section below, and will only have to point OIIO build process so their locations.

* Boost: get the boost source archive, extract into `{BOOST_ROOT}`.
  ```
  cd {BOOST_ROOT}
  bootstrap
  b2
  ```
* zlib: this will build it, and then delete the non-static library, so they don't get picked up:
  ```
  cd {ZLIB_ROOT}
  git clone https://github.com/madler/zlib .
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=.
  cmake --build build --config Release --target install
  del build\lib\zlib.lib
  ```
* libTIFF:
  ```
  cd {TIFF_ROOT}
  git clone https://gitlab.com/libtiff/libtiff.git .
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX=.
  cmake --build build --target install
  ```
* libjpeg-turbo:
  ```
  cd {JPEG_ROOT}
  git clone https://github.com/libjpeg-turbo/libjpeg-turbo .
  mkdir build
  cd build
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_SHARED=OFF -DCMAKE_INSTALL_PREFIX=.
  cmake --build build --config Release --target install
  ```
* OpenEXR: you'll have to point it to your `{ZLIB_ROOT}` location from the above. If copy-pasting the multi-line command (with lines ending in `^`) into
  cmd.exe prompt, make sure to copy all the lines at once.
  ```
  cd {EXR_ROOT}
  git clone https://github.com/AcademySoftwareFoundation/openexr .
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=dist ^
    -DBUILD_TESTING=OFF -DBUILD_SHARED_LIBS=OFF -DOPENEXR_BUILD_TOOLS=OFF ^
    -DOPENEXR_INSTALL_TOOLS=OFF -DOPENEXR_INSTALL_EXAMPLES=OFF ^
    -DZLIB_ROOT={ZLIB_ROOT}\build
  cmake --build build --target install --config Release
  ```

Now get the OIIO source and do one-time CMake configuration step. Replace `{*_ROOT}` below with folders where you have put the 3rd party
dependencies.
```
cd {OIIO_ROOT}
git clone https://github.com/OpenImageIO/oiio .
cmake -S . -B build -DVERBOSE=ON -DCMAKE_BUILD_TYPE=Release ^
  -DBoost_USE_STATIC_LIBS=ON -DBoost_NO_WARN_NEW_VERSIONS=ON -DBoost_ROOT={BOOST_ROOT} ^
  -DZLIB_ROOT={ZLIB_ROOT}\build ^
  -DTIFF_ROOT={TIFF_ROOT}\build ^
  -DOpenEXR_ROOT={EXR_ROOT}\build\dist ^
  -DImath_DIR={EXR_ROOT}\build\dist\lib\cmake\Imath ^
  -DJPEG_ROOT={JPEG_ROOT}\build ^
  -DUSE_PYTHON=0 -DUSE_QT=0 -DBUILD_SHARED_LIBS=0 -DLINKSTATIC=1
```

This will produce `{OIIO_ROOT}/build/OpenImageIO.sln` that can be opened in Visual Studio IDE. Note that the solution will be
only for the Intel x64 architecture only; and will only target "min-spec" (SSE2) SIMD instruction set.

Optional packages that OIIO can use (e.g. libpng, Qt) can be build and pointed to OIIO build process in a similar way.

In Visual Studio, open `{OIIO_ROOT}/build/OpenImageIO.sln` and pick Release build configuration. If you pick Debug, you
might need to re-run the CMake command above with `-DCMAKE_BUILD_TYPE=Debug` and also have all the dependencies above built
with `Debug` config too.

The main project that builds the library is `OpenImageIO`. The library is built into `{OIIO_ROOT}/build/lib/{CONFIG}` folder.
The various OIIO command line tools (`oiiotool`, `iconvert` etc.) are projects under Tools subfolder in VS IDE solution explorer.
They all build into `{OIIO_ROOT}/build/bin/{CONFIG}` folder.

There's a `CMakePredefinedTargets/INSTALL` project that you can build to produce a `{OIIO_ROOT}/dist` folder with `bin`, `include`,
`lib`, `share` folders as an OIIO distribution.

The instructions above use options for building statically-linked OIIO library and tools. Adjust options passed to CMake to
produce a dynamic-linked version.


**Method 2 - Using vcpkg**

1. Visit Microsoft's vcpkg GitHub page: https://github.com/Microsoft/vcpkg. Also note that the openimageio package is located here: https://github.com/Microsoft/vcpkg/tree/master/ports/openimageio

2. Follow vcpkg installation instructions and complete the install. Please note vcpkg has its own list of prerequisites listed on their page.

3. Execute the PowerShell command from where vcpkg is located in directory. ``vcpkg install openimageio``

Test Images
-----------

We have yet another project containing a set of sample images for testing
OpenImageIO. We split test images into a separate project in order to make
the main source code tree smaller and simpler for people who don't need the
test suite.

    git clone https://github.com/OpenImageIO/oiio-images.git

Also, there are collections of images for some of the file formats we
support, and make test expects them to also be present. To run full tests,
you will need to download and unpack the test image collections from:

* http://www.simplesystems.org/libtiff/images.html
* http://www.openexr.com/downloads.html
* http://www.itu.int/net/ITU-T/sigdb/speimage/ImageForm-s.aspx?val=10100803
* http://www.cv.nrao.edu/fits/data/tests/

These images should be placed in a sibling directory to the OpenImageIO
repository named oiio-testimages.

You do not need any of these packages in order to build or use
OpenImageIO. But if you are going to contribute to OpenImageIO
development, you probably want them, since it is required for executing
OpenImageIO's test suite (when you run "make test").

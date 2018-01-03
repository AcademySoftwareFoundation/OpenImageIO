Installation instructions for OpenImageIO
=========================================

For the most up-to-date build instructions (and in any case somewhat
more detailed than here), please see our wiki:

https://sites.google.com/site/openimageio/checking-out-and-building-openimageio

Supported platforms at present include Linux (32 and 64 bit),
Mac OS X, and Windows.

Our build system is based upon 'CMake'.  If you don't already have it
installed on your system, you can get it from http://www.cmake.org

After you build OpenImageIO, if you compiled with the EMBEDPLUGINS=0 flag
you will need to set the environment variable OIIO_LIBRARY_PATH to point
to the 'lib' directory where OpenImageIO is installed, or else it will
not be able to find the plugins.


Dependencies
------------

NEW or CHANGED dependencies since the last major release are **bold**.

### Required dependencies -- OIIO will not build at all without these

 * C++11 (should also build with C++14 and C++17)
 * Compilers: gcc 4.8.2 - gcc 7, clang 3.3 - 5.0, MSVS 2013 - 2017, icc version 13 or higher
 * Boost >= 1.53 (tested up through 1.65)
 * CMake >= 3.2.2 (tested up through 3.9)
 * OpenEXR >= 2.0 (recommended: 2.2)
 * libTIFF >= 3.9 (recommended: 4.0+)

### Optional dependencies -- features may be disabled if not found
 * If you are building the `iv` viewer (which will be disabled if any of
   these are not found):
     * Qt >= 5.6
     * OpenGL
 * If you are building the Python bindings:
     * Python >= 2.7
     * **NumPy**
     * **pybind11** (but OIIO will auto-download it if not found)
 * libRaw >= 0.17 ("RAW" image reading will be disabled if not found)



Building OpenImageIO on Linux or OS X
-------------------------------------

The following dependencies must be installed to build the core of
OpenImageIO: Boost, libjpeg, libtiff, libpng and OpenEXR.  These can be
installed using the standard package managers on your system.
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

On Linux and OS X, you can build from source from the top-level
directory by just typing 'make'.  (Yes, we have a 'make' wrapper around
our CMake build, it simplifies things.)

During the make, various temporary files (object files, etc.) will
be put in build/PLATFORM, where 'PLATFORM' will be the name of the
platform you are building for (e.g., linux, linux64, macosx).

The result of the make will be a full binary distribution in 
dist/PLATFORM.

Make targets you should know about:

|  Target             |  Command                                           |
| ------------------- | -------------------------------------------------- |
|  make               |  Build an optimized distro in dist/PLATFORM, with temp files created while building in build/PLATFORM.
|  make debug         |  Build a debugging (symbols, not stripped) distro, will end up in dist/PLATFORM.debug
|  make clean         |  Get rid of all the temporary files in build/PLATFORM
|  make realclean     |  Get rid of both build/PLATFORM and dist/PLATFORM
|  make nuke          |  Get rid of all build/ and dist/, for all platforms
|  make profile       |  Build a profilable version dist/PLATFORM.profile
|  make doxygen       |  Build the Doxygen docs
|  make help          |  Print all the make options

Additionally, a few helpful modifiers alter some build-time options:

|  Target                   |  Command                                           |
| :------------------------ | -------------------------------------------------- |
| make VERBOSE=1 ...        |  Show all compilation commands
| make STOP_ON_WARNING=0    |  Do not stop building if compiler warns
| make EMBEDPLUGINS=0 ...   |  Don't compile the plugins into libOpenImageIO
| make USE_OPENGL=0 ...     |  Skip anything that needs OpenGL
| make USE_QT=0 ...         |  Skip anything that needs Qt
| make MYCC=xx MYCXX=yy ... |  Use custom compilers
| make USE_PYTHON=0 ...     |  Don't build the Python binding
| make BUILDSTATIC=1 ...    |  Build static library instead of shared
| make LINKSTATIC=1 ...     |  Link with static external libraries when possible
| make SOVERSION=nn ...     |  Include the specifed major version number in the shared object metadata
| make NAMESPACE=name       |   Wrap everything in another namespace

The command 'make help' will list all possible options.

You can also ignore the top level Makefile wrapper, and instead use
CMake directly:

    mkdir build
    cd build
    cmake ..

If the compile stops because of warnings, try again with 

    make nuke
    make STOP_ON_WARNING=0

or, if you are using CMake directly,

    cd build
    cmake -DSTOP_ON_WARNING=0 ..




Building on Windows
-------------------

See the latest Windows build docs on our web site:
https://sites.google.com/site/openimageio/building-oiio-on-windows

1. Check out the trunk or a branch of your choice.  The remainder of
   these instructions assume that you checked out the trunk to the
   D:\OIIO\trunk directory.

2. Download the package with precompiled external libraries from
http://www.openimageio.org/external.zip

   Next, unpack it. The directory with downloaded code from the repository
   and the directory with unpacked libraries should be siblings. For example,

   ```
   D:
       \OIIO
           \trunk          // this is my tree
               \src        // directory with src files for OIIO
               \build      // directory that is created by cmake
           \external       // this is extracted external package
               \dist
                   \windows
                       \glew-1.5.1
                       \ilmbase-1.0.1
                       \jpeg-6b
                       \libpng-1.2.3
                       \openexr-1.6.1
                       \tbb-21_20080605oss
                       \zlib-1.2.3
                       \tiff-3.8.2
   ```

3. Download precompiled Qt4 binaries for Windows from here:
   http://qt.windows.binaries.googlepages.com/index.html

   Unpack it (it doesn't matter where). After unpacking, add the path to
   the Qt bin directory to the PATH variable. For example, if you unpacked
   this package to the D:\qt-win directory, you should add D:\qt-win\bin to
   your PATH. It's important to add the Qt bin directory to PATH because
   the FindQt4 module uses it to search for qmake applications.

4. Also, just to be safe, add QTDIR to the environment variables. It
   should point to directory where you unpacked Qt.

5. Download precompiled BOOST 1.53 or newer libraries from
   http://www.boostpro.com/download

   Install it on your system. Choose two versions: Multithread Debug, DLL
   and Multithread, DLL for Your Visual Studio version.

6. Download precompiled BOOST 1.53 or newer libraries from here (unfficial
   mirror) or from here (unofficial mirror, registration required). Install
   it on Your system. Choose two versions: Multithread Debug, DLL and
   Multithread, DLL for Your Visual Studio version.

7. Install cmake. You can download precompiled binaries from here:
   http://www.cmake.org/cmake/resources/software.html
   After installing, run cmake-gui. Set the field that specifies the source
   code location (for example, to D:\OIIO\trunk\src). Then set the field
   that specifies where to build binaries to the directory you want to
   build project for OIIO (for example, D:\OIIO\trunk\build).

8. Set the THIRD_PARTY_TOOLS_HOME environment variable to the directory
   where are stored unpacked external libraries (for example,
   D:\OIIO\external\dist\windows). You can add variables by clicking Add
   entry button.

9. Hit the Configure button. Cmake should automatically find externals
   libraries and prepare the environment for creating the OIIO project. If
   the configuration process ends without errors go to next step. If not,
   read the instructions from the end of this tutorial.

10. Hit the Generate button. Cmake will build Visual Studio a solution in
the build directory.

11. That's all. You can open the OpenImageIO solution and start developing
OIIO! Potential problems:


It may happen that cmake won't find zlib, png, tiff or jpeg
libraries. If so you have to set CMAKE_PREFIX_PATH to point to the
directory where the missing libraries are stored. For example, if cmake
can't find ZLIB, add to CMAKE_PREFIX_PATH the
D:\OIIO\external\dist\windows\zlib-1.2.3 directory. If it can't find
ZLIB and PNG, add
D:\OIIO\external\dist\windows\zlib-1.2.3;D:\OIIO\external\dist\windows\libpng-1.2.3.


Test Images
-----------

We have yet another SVN project just for containing a set of sample
images for testing OpenImageIO. We split test images into a separate
SVN project in order to make the main source code tree smaller and
simpler for people who don't need the test suite.

    git clone https://github.com/OpenImageIO/oiio-images.git

Also, there are collections of images for some of the file formats we
support, and make test expects them to also be present. To run full tests,
you will need to download and unpack the test image collections from:

* http://www.remotesensing.org/libtiff/images.html
* http://www.openexr.com/downloads.html
* http://www.itu.int/net/ITU-T/sigdb/speimage/ImageForm-s.aspx?val=10100803
* http://www.cv.nrao.edu/fits/data/tests/

These images should be placed in a sibling directory to the OpenImageIO
repository named oiio-testimages.

You do not need any of these packages in order to build or use
OpenImageIO. But if you are going to contribute to OpenImageIO
development, you probably want them, since it is required for executing
OpenImageIO's test suite (when you run "make test").

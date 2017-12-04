#########################################################################
#
# This is the master makefile.
# Here we put all the top-level make targets, platform-independent
# rules, etc.
#
# Run 'make help' to list helpful targets.
#
#########################################################################


.PHONY: all debug profile clean realclean nuke doxygen

working_dir	:= ${shell pwd}

# Figure out which architecture we're on
include ${working_dir}/src/make/detectplatform.mk

# Presence of make variables DEBUG and PROFILE cause us to make special
# builds, which we put in their own areas.
ifdef DEBUG
    variant +=.debug
endif
ifdef PROFILE
    variant +=.profile
endif

MY_MAKE_FLAGS ?=
MY_NINJA_FLAGS ?=
MY_CMAKE_FLAGS += -g3 -DSELF_CONTAINED_INSTALL_TREE:BOOL=TRUE
BUILDSENTINEL ?= Makefile
NINJA ?= ninja
CMAKE ?= cmake

# Site-specific build instructions
ifndef OPENIMAGEIO_SITE
    OPENIMAGEIO_SITE := ${shell uname -n}
endif
ifneq (${shell echo ${OPENIMAGEIO_SITE} | grep imageworks.com},)
include ${working_dir}/site/spi/Makefile-bits
endif

# Set up variables holding the names of platform-dependent directories --
# set these after evaluating site-specific instructions
top_build_dir := build
build_dir     := ${top_build_dir}/${platform}${variant}
top_dist_dir  := dist
dist_dir      := ${top_dist_dir}/${platform}${variant}

ifndef INSTALL_PREFIX
INSTALL_PREFIX := ${working_dir}/${dist_dir}
INSTALL_PREFIX_BRIEF := ${dist_dir}
else
INSTALL_PREFIX_BRIEF := ${INSTALL_PREFIX}
endif

VERBOSE ?= ${SHOWCOMMANDS}
ifneq (${VERBOSE},)
MY_MAKE_FLAGS += VERBOSE=${VERBOSE}
MY_CMAKE_FLAGS += -DVERBOSE:BOOL=${VERBOSE}
ifneq (${VERBOSE},0)
	MY_NINJA_FLAGS += -v
	TEST_FLAGS += -V
endif
$(info OPENIMAGEIO_SITE = ${OPENIMAGEIO_SITE})
$(info dist_dir = ${dist_dir})
$(info INSTALL_PREFIX = ${INSTALL_PREFIX})
endif

ifneq (${EMBEDPLUGINS},)
MY_CMAKE_FLAGS += -DEMBEDPLUGINS:BOOL=${EMBEDPLUGINS}
endif

ifneq (${USE_OPENGL},)
MY_CMAKE_FLAGS += -DUSE_OPENGL:BOOL=${USE_OPENGL}
endif

ifneq (${USE_QT},)
MY_CMAKE_FLAGS += -DUSE_QT:BOOL=${USE_QT}
endif

ifneq (${FORCE_OPENGL_1},)
MY_CMAKE_FLAGS += -DFORCE_OPENGL_1:BOOL=${FORCE_OPENGL_1}
endif

ifneq (${OIIO_THREAD_ALLOW_DCLP},)
MY_CMAKE_FLAGS += -DOIIO_THREAD_ALLOW_DCLP:BOOL=${OIIO_THREAD_ALLOW_DCLP}
endif

ifneq (${NAMESPACE},)
MY_CMAKE_FLAGS += -DOIIO_NAMESPACE:STRING=${NAMESPACE}
endif

ifneq (${HIDE_SYMBOLS},)
MY_CMAKE_FLAGS += -DHIDE_SYMBOLS:BOOL=${HIDE_SYMBOLS}
endif

ifneq (${USE_PYTHON},)
MY_CMAKE_FLAGS += -DUSE_PYTHON:BOOL=${USE_PYTHON}
endif

ifneq (${USE_PYTHON3},)
MY_CMAKE_FLAGS += -DUSE_PYTHON3:BOOL=${USE_PYTHON3}
endif

ifneq (${PYTHON_VERSION},)
MY_CMAKE_FLAGS += -DPYTHON_VERSION:STRING=${PYTHON_VERSION}
endif

ifneq (${PYLIB_LIB_PREFIX},)
MY_CMAKE_FLAGS += -DPYLIB_LIB_PREFIX:BOOL=${PYLIB_LIB_PREFIX}
endif

ifneq (${PYLIB_INCLUDE_SONAME},)
MY_CMAKE_FLAGS += -DPYLIB_INCLUDE_SONAME:BOOL=${PYLIB_INCLUDE_SONAME}
endif

ifneq (${USE_FFMPEG},)
MY_CMAKE_FLAGS += -DUSE_FFMPEG:BOOL=${USE_FFMPEG}
endif

ifneq (${USE_FIELD3D},)
MY_CMAKE_FLAGS += -DUSE_FIELD3D:BOOL=${USE_FIELD3D}
endif

ifneq (${FIELD3D_HOME},)
MY_CMAKE_FLAGS += -DFIELD3D_HOME:STRING=${FIELD3D_HOME}
endif

ifneq (${USE_OPENJPEG},)
MY_CMAKE_FLAGS += -DUSE_OPENJPEG:BOOL=${USE_OPENJPEG}
endif

ifneq (${OPENJPEG_HOME},)
MY_CMAKE_FLAGS += -DOPENJPEG_HOME:STRING=${OPENJPEG_HOME}
endif

ifneq (${USE_JPEGTURBO},)
MY_CMAKE_FLAGS += -DUSE_JPEGTURBO:BOOL=${USE_JPEGTURBO}
endif

ifneq (${JPEGTURBO_PATH},)
MY_CMAKE_FLAGS += -DJPEGTURBO_PATH:STRING=${JPEGTURBO_PATH}
endif

ifneq (${USE_GIF},)
MY_CMAKE_FLAGS += -DUSE_GIF:BOOL=${USE_GIF}
endif

ifneq (${GIF_DIR},)
MY_CMAKE_FLAGS += -DGIF_DIR:STRING=${GIF_DIR}
endif

ifneq (${USE_OCIO},)
MY_CMAKE_FLAGS += -DUSE_OCIO:BOOL=${USE_OCIO}
endif

ifneq (${USE_NUKE},)
MY_CMAKE_FLAGS += -DUSE_NUKE:BOOL=${USE_NUKE}
endif

ifneq (${USE_OPENCV},)
MY_CMAKE_FLAGS += -DUSE_OPENCV:BOOL=${USE_OPENCV}
endif

ifneq (${USE_LIBRAW},)
MY_CMAKE_FLAGS += -DUSE_LIBRAW:BOOL=${USE_LIBRAW}
endif

ifneq (${LIBRAW_PATH},)
MY_CMAKE_FLAGS += -DLIBRAW_PATH:STRING=${LIBRAW_PATH}
endif

ifneq (${USE_PTEX},)
MY_CMAKE_FLAGS += -DUSE_PTEX:BOOL=${USE_PTEX}
endif

ifneq (${USE_EXTERNAL_PUGIXML},)
MY_CMAKE_FLAGS += -DUSE_EXTERNAL_PUGIXML:BOOL=${USE_EXTERNAL_PUGIXML} -DPUGIXML_HOME=${PUGIXML_HOME}
endif

ifneq (${OPENEXR_HOME},)
MY_CMAKE_FLAGS += -DOPENEXR_HOME:STRING=${OPENEXR_HOME}
endif

ifneq (${ILMBASE_HOME},)
MY_CMAKE_FLAGS += -DILMBASE_HOME:STRING=${ILMBASE_HOME}
endif

ifneq (${OCIO_HOME},)
MY_CMAKE_FLAGS += -DOCIO_PATH:STRING=${OCIO_HOME}
endif

ifneq (${BOOST_HOME},)
MY_CMAKE_FLAGS += -DBOOST_ROOT:STRING=${BOOST_HOME}
endif

ifneq (${NUKE_HOME},)
MY_CMAKE_FLAGS += -DNuke_ROOT:STRING=${NUKE_HOME}
endif

ifneq (${NUKE_VERSION},)
MY_CMAKE_FLAGS += -DNUKE_VERSION:STRING=${NUKE_VERSION}
endif

ifneq (${STOP_ON_WARNING},)
MY_CMAKE_FLAGS += -DSTOP_ON_WARNING:BOOL=${STOP_ON_WARNING}
endif

ifneq (${BUILDSTATIC},)
MY_CMAKE_FLAGS += -DBUILDSTATIC:BOOL=${BUILDSTATIC}
endif

ifneq (${LINKSTATIC},)
MY_CMAKE_FLAGS += -DLINKSTATIC:BOOL=${LINKSTATIC}
endif

ifneq (${OIIO_BUILD_TOOLS},)
MY_CMAKE_FLAGS += -DOIIO_BUILD_TOOLS:BOOL=${OIIO_BUILD_TOOLS}
endif

ifneq (${OIIO_BUILD_TESTS},)
MY_CMAKE_FLAGS += -DOIIO_BUILD_TESTS:BOOL=${OIIO_BUILD_TESTS}
endif

ifneq (${SOVERSION},)
MY_CMAKE_FLAGS += -DSOVERSION:STRING=${SOVERSION}
endif

ifneq (${BUILD_OIIOUTIL_ONLY},)
MY_CMAKE_FLAGS += -DBUILD_OIIOUTIL_ONLY:BOOL=${BUILD_OIIOUTIL_ONLY}
endif

ifdef DEBUG
MY_CMAKE_FLAGS += -DCMAKE_BUILD_TYPE:STRING=Debug
endif

ifdef PROFILE
MY_CMAKE_FLAGS += -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo
endif

ifneq (${MYCC},)
MY_CMAKE_FLAGS += -DCMAKE_C_COMPILER:STRING="${MYCC}"
endif
ifneq (${MYCXX},)
MY_CMAKE_FLAGS += -DCMAKE_CXX_COMPILER:STRING="${MYCXX}"
endif

ifneq (${USE_CPP},)
MY_CMAKE_FLAGS += -DUSE_CPP=${USE_CPP}
endif

ifneq (${USE_LIBCPLUSPLUS},)
MY_CMAKE_FLAGS += -DUSE_LIBCPLUSPLUS:BOOL=${USE_LIBCPLUSPLUS}
endif

ifneq (${EXTRA_CPP_ARGS},)
MY_CMAKE_FLAGS += -DEXTRA_CPP_ARGS:STRING="${EXTRA_CPP_ARGS}"
endif

ifneq (${USE_SIMD},)
MY_CMAKE_FLAGS += -DUSE_SIMD:STRING="${USE_SIMD}"
endif

ifneq (${TEX_BATCH_SIZE},)
MY_CMAKE_FLAGS += -DTEX_BATCH_SIZE:STRING="${TEX_BATCH_SIZE}"
endif

ifneq (${TEST},)
TEST_FLAGS += -R ${TEST}
endif

ifneq (${USE_CCACHE},)
MY_CMAKE_FLAGS += -DUSE_CCACHE:BOOL=${USE_CCACHE}
endif

ifeq (${USE_NINJA},1)
MY_CMAKE_FLAGS += -G Ninja
BUILDSENTINEL := build.ninja
endif

ifeq (${CODECOV},1)
MY_CMAKE_FLAGS += -DCMAKE_BUILD_TYPE:STRING=Debug -DCODECOV:BOOL=${CODECOV}
endif

ifneq (${SANITIZE},)
MY_CMAKE_FLAGS += -DSANITIZE=${SANITIZE}
endif

ifneq (${CLANG_TIDY},)
  MY_CMAKE_FLAGS += -DCLANG_TIDY:BOOL=1
endif
ifneq (${CLANG_TIDY_CHECKS},)
  MY_CMAKE_FLAGS += -DCLANG_TIDY_CHECKS:STRING=${CLANG_TIDY_CHECKS}
endif
ifneq (${CLANG_TIDY_ARGS},)
  MY_CMAKE_FLAGS += -DCLANG_TIDY_ARGS:STRING=${CLANG_TIDY_ARGS}
endif
ifneq (${CLANG_TIDY_FIX},)
  MY_CMAKE_FLAGS += -DCLANG_TIDY_FIX:BOOL=${CLANG_TIDY_FIX}
  MY_NINJA_FLAGS += -j 1
  # N.B. when fixing, you don't want parallel jobs!
endif

ifneq (${USE_FREETYPE},)
MY_CMAKE_FLAGS += -DUSE_FREETYPE:BOOL=${USE_FREETYPE}
endif

ifneq (${BUILD_MISSING_DEPS},)
MY_CMAKE_FLAGS += -DBUILD_MISSING_DEPS:BOOL=${BUILD_MISSING_DEPS}
endif


#$(info MY_CMAKE_FLAGS = ${MY_CMAKE_FLAGS})
#$(info MY_MAKE_FLAGS = ${MY_MAKE_FLAGS})

#########################################################################




#########################################################################
# Top-level documented targets

all: dist

# 'make debug' is implemented via recursive make setting DEBUG
debug:
	${MAKE} DEBUG=1 --no-print-directory

# 'make profile' is implemented via recursive make setting PROFILE
profile:
	${MAKE} PROFILE=1 --no-print-directory

# 'make cmakesetup' constructs the build directory and runs 'cmake' there,
# generating makefiles to build the project.  For speed, it only does this when
# ${build_dir}/Makefile doesn't already exist, in which case we rely on the
# cmake generated makefiles to regenerate themselves when necessary.
cmakesetup:
	@ (if [ ! -e ${build_dir}/${BUILDSENTINEL} ] ; then \
		${CMAKE} -E make_directory ${build_dir} ; \
		cd ${build_dir} ; \
		${CMAKE} -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} \
			${MY_CMAKE_FLAGS} -DBOOST_ROOT=${BOOST_HOME} \
			../.. ; \
	 fi)

ifeq (${USE_NINJA},1)

# 'make cmake' does a basic build (after first setting it up)
cmake: cmakesetup
	@ ( cd ${build_dir} ; ${NINJA} ${MY_NINJA_FLAGS} )

# 'make cmakeinstall' builds everthing and installs it in 'dist'.
# Suppress pointless output from docs installation.
cmakeinstall: cmake
	@ ( cd ${build_dir} ; ${NINJA} ${MY_NINJA_FLAGS} install | grep -v '^-- \(Installing\|Up-to-date\|Set runtime path\)' )

# 'make package' builds everything and then makes an installable package
# (platform dependent -- may be .tar.gz, .sh, .dmg, .rpm, .deb. .exe)
package: cmakeinstall
	@ ( cd ${build_dir} ; ${NINJA} ${MY_NINJA_FLAGS} package )

# 'make package_source' makes an installable source package
# (platform dependent -- may be .tar.gz, .sh, .dmg, .rpm, .deb. .exe)
package_source: cmakeinstall
	@ ( cd ${build_dir} ; ${NINJA} ${MY_NINJA_FLAGS} package_source )

else

# 'make cmake' does a basic build (after first setting it up)
cmake: cmakesetup
	@ ( cd ${build_dir} ; ${MAKE} ${MY_MAKE_FLAGS} )

# 'make cmakeinstall' builds everthing and installs it in 'dist'.
# Suppress pointless output from docs installation.
cmakeinstall: cmake
	@ ( cd ${build_dir} ; ${MAKE} ${MY_MAKE_FLAGS} install | grep -v '^-- \(Installing\|Up-to-date\|Set runtime path\)' )

# 'make package' builds everything and then makes an installable package
# (platform dependent -- may be .tar.gz, .sh, .dmg, .rpm, .deb. .exe)
package: cmakeinstall
	@ ( cd ${build_dir} ; ${MAKE} ${MY_MAKE_FLAGS} package )

# 'make package_source' makes an installable source package
# (platform dependent -- may be .tar.gz, .sh, .dmg, .rpm, .deb. .exe)
package_source: cmakeinstall
	@ ( cd ${build_dir} ; ${MAKE} ${MY_MAKE_FLAGS} package_source )

endif

# 'make dist' is just a synonym for 'make cmakeinstall'
dist : cmakeinstall

TEST_FLAGS += --force-new-ctest-process --output-on-failure

# 'make test' does a full build and then runs all tests
test: cmake
	@ ${CMAKE} -E cmake_echo_color --switch=$(COLOR) --cyan "Running tests ${TEST_FLAGS}..."
	@ ( cd ${build_dir} ; PYTHONPATH=${PWD}/${build_dir}/src/python ctest -E broken ${TEST_FLAGS} )
	@ ( if [ "${CODECOV}" == "1" ] ; then \
	      cd ${build_dir} ; \
	      lcov -b . -d . -c -o cov.info ; \
	      lcov --remove cov.info "/usr*" -o cov.info ; \
	      genhtml -o ./cov -t "OIIO test coverage" --num-spaces 4 cov.info ; \
	  fi )

# 'make testall' does a full build and then runs all tests (even the ones
# that are expected to fail on some platforms)
testall: cmake
	${CMAKE} -E cmake_echo_color --switch=$(COLOR) --cyan "Running all tests ${TEST_FLAGS}..."
	( cd ${build_dir} ; PYTHONPATH=${PWD}/${build_dir}/src/python ctest ${TEST_FLAGS} )

# 'make clean' clears out the build directory for this platform
clean:
	${CMAKE} -E remove_directory ${build_dir}

# 'make realclean' clears out both build and dist directories for this platform
realclean: clean
	${CMAKE} -E remove_directory ${dist_dir}

# 'make nuke' blows away the build and dist areas for all platforms
nuke:
	${CMAKE} -E remove_directory ${top_build_dir}
	${CMAKE} -E remove_directory ${top_dist_dir}

doxygen:
	doxygen src/doc/Doxyfile

#########################################################################



# 'make help' prints important make targets
help:
	@echo "Targets:"
	@echo "  make              Build optimized binaries and libraries"
	@echo "  make debug        Build unoptimized with symbols"
	@echo "  make profile      Build for profiling"
	@echo "  make clean        Remove the temporary files in ${build_dir}"
	@echo "  make realclean    Remove both ${build_dir} AND ${dist_dir}"
	@echo "  make nuke         Remove ALL of build and dist (not just ${platform})"
	@echo "  make test         Run tests"
	@echo "  make testall      Run all tests, even broken ones"
	@echo "  make doxygen      Build the Doxygen docs in ${top_build_dir}/doxygen"
	@echo ""
	@echo "Helpful modifiers:"
	@echo "  C++ compiler and build process:"
	@echo "      VERBOSE=1                Show all compilation commands"
	@echo "      STOP_ON_WARNING=0        Do not stop building if compiler warns"
	@echo "      OPENIMAGEIO_SITE=xx      Use custom site build mods"
	@echo "      MYCC=xx MYCXX=yy         Use custom compilers"
	@echo "      USE_CPP=14               Compile in C++14 mode (default is C++11)"
	@echo "      USE_LIBCPLUSPLUS=1       Use clang libc++"
	@echo "      EXTRA_CPP_ARGS=          Additional args to the C++ command"
	@echo "      USE_NINJA=1              Set up Ninja build (instead of make)"
	@echo "      USE_CCACHE=0             Disable ccache (even if available)"
	@echo "      CODECOV=1                Enable code coverage tests"
	@echo "      SANITIZE=name1,...       Enable sanitizers (address, leak, thread)"
	@echo "      CLANG_TIDY=1             Run clang-tidy on all source (can be modified"
	@echo "                                  by CLANG_TIDY_ARGS=... and CLANG_TIDY_FIX=1"
	@echo "  Linking and libraries:"
	@echo "      HIDE_SYMBOLS=1           Hide symbols not in the public API"
	@echo "      SOVERSION=nn             Include the specifed major version number "
	@echo "                                  in the shared object metadata"
	@echo "      BUILDSTATIC=1            Build static library instead of shared"
	@echo "      LINKSTATIC=1             Link with static external libs when possible"
	@echo "  Finding and Using Dependencies:"
	@echo "      BOOST_HOME=path          Custom Boost installation"
	@echo "      OPENEXR_HOME=path        Custom OpenEXR installation"
	@echo "      ILMBASE_HOME=path        Custom IlmBase installation"
	@echo "      USE_EXTERNAL_PUGIXML=1   Use the system PugiXML, not the one in OIIO"
	@echo "      USE_QT=0                 Skip anything that needs Qt"
	@echo "      USE_OPENGL=0             Skip anything that needs OpenGL"
	@echo "      FORCE_OPENGL_1=1         Force iv to use OpenGL's fixed pipeline"
	@echo "      USE_PYTHON=0             Don't build the Python binding"
	@echo "      USE_PYTHON3=1            If 1, try to build against Python3, not 2.x"
	@echo "      PYTHON_VERSION=2.6       Specify the Python version"
	@echo "      USE_FIELD3D=0            Don't build the Field3D plugin"
	@echo "      FIELD3D_HOME=path        Custom Field3D installation"
	@echo "      USE_FFMPEG=0             Don't build the FFmpeg plugin"
	@echo "      USE_JPEGTURBO=0          Don't build the JPEG-Turbo even if found"
	@echo "      JPEGTURBO_PATH=path      Custom path for JPEG-Turbo"
	@echo "      USE_OPENJPEG=0           Don't build the JPEG-2000 plugin"
	@echo "      USE_GIF=0                Don't build the GIF plugin"
	@echo "      GIF_DIR=path             Custom GIFLIB installation"
	@echo "      USE_OCIO=0               Don't use OpenColorIO even if found"
	@echo "      OCIO_HOME=path           Custom OpenColorIO installation"
	@echo "      USE_NUKE=0               Don't build Nuke plugins"
	@echo "      NUKE_HOME=path           Custom Nuke installation"
	@echo "      NUKE_VERSION=ver         Custom Nuke version"
	@echo "      USE_LIBRAW=0             Don't use LibRaw, even if found"
	@echo "      LIBRAW_PATH=path         Custom LibRaw installation"
	@echo "      USE_OPENCV=0             Skip anything that needs OpenCV"
	@echo "      USE_PTEX=0               Skip anything that needs PTex"
	@echo "      USE_FREETYPE=0           Skip anything that needs Freetype"
	@echo "  OIIO build-time options:"
	@echo "      INSTALL_PREFIX=path      Set installation prefix (default: ./${INSTALL_PREFIX_BRIEF})"
	@echo "      NAMESPACE=name           Override namespace base name (default: OpenImageIO)"
	@echo "      EMBEDPLUGINS=0           Don't compile the plugins into libOpenImageIO"
	@echo "      OIIO_THREAD_ALLOW_DCLP=0 Don't allow threads.h to use DCLP"
	@echo "      OIIO_BUILD_TOOLS=0       Skip building the command-line tools"
	@echo "      OIIO_BUILD_TESTS=0       Skip building the unit tests"
	@echo "      BUILD_OIIOUTIL_ONLY=1    Build *only* libOpenImageIO_Util"
	@echo "      USE_SIMD=arch            Build with SIMD support (comma-separated choices:"
	@echo "                                  0, sse2, sse3, ssse3, sse4.1, sse4.2, f16c,"
	@echo "                                  avx, avx2, avx512f)"
	@echo "      TEX_BATCH_SIZE=16        Override TextureSystem SIMD batch size"
	@echo "      BUILD_MISSING_DEPS=1     Try to download/build missing dependencies"
	@echo "  make test, extra options:"
	@echo "      TEST=regex               Run only tests matching the regex"
	@echo ""


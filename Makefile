#########################################################################
#
# This is the master makefile.
# Here we put all the top-level make targets, platform-independent
# rules, etc. This is just a fancy wrapper around cmake, but for many
# people, it's a lot simpler to just type "make" and have everything
# happen automatically.
#
# Run 'make help' to list helpful targets.
#
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO
#########################################################################


.PHONY: all debug profile clean realclean nuke doxygen

working_dir	:= ${shell pwd}

# Figure out which architecture we're on
include ${working_dir}/src/make/detectplatform.mk

MY_MAKE_FLAGS ?=
MY_NINJA_FLAGS ?=
MY_CMAKE_FLAGS ?=
BUILDSENTINEL ?= Makefile
NINJA ?= ninja
CMAKE ?= cmake
CMAKE_BUILD_TYPE ?= Release

# Site-specific build instructions
OPENIMAGEIO_SITE ?= ${shell uname -n}
ifneq (${shell echo ${OPENIMAGEIO_SITE} | grep imageworks.com},)
include ${working_dir}/site/spi/Makefile-bits
endif

# Set up variables holding the names of platform-dependent directories --
# set these after evaluating site-specific instructions
build_dir ?= build
dist_dir  ?= dist

INSTALL_PREFIX ?= ${working_dir}/${dist_dir}

VERBOSE ?= ${SHOWCOMMANDS}
ifneq (${VERBOSE},)
MY_MAKE_FLAGS += VERBOSE=${VERBOSE}
MY_CMAKE_FLAGS += -DVERBOSE:BOOL=${VERBOSE} --log-level=VERBOSE
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

ifneq (${OIIO_THREAD_ALLOW_DCLP},)
MY_CMAKE_FLAGS += -DOIIO_THREAD_ALLOW_DCLP:BOOL=${OIIO_THREAD_ALLOW_DCLP}
endif

ifneq (${NAMESPACE},)
MY_CMAKE_FLAGS += -DOIIO_NAMESPACE:STRING=${NAMESPACE}
endif

ifneq (${USE_PYTHON},)
MY_CMAKE_FLAGS += -DUSE_PYTHON:BOOL=${USE_PYTHON}
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

ifneq (${USE_EXTERNAL_PUGIXML},)
MY_CMAKE_FLAGS += -DUSE_EXTERNAL_PUGIXML:BOOL=${USE_EXTERNAL_PUGIXML}
endif

ifneq (${OPENEXR_ROOT},)
MY_CMAKE_FLAGS += -DOPENEXR_ROOT:STRING=${OPENEXR_ROOT}
endif

ifneq (${ILMBASE_ROOT},)
MY_CMAKE_FLAGS += -DILMBASE_ROOT:STRING=${ILMBASE_ROOT}
endif

ifneq (${NUKE_VERSION},)
MY_CMAKE_FLAGS += -DNUKE_VERSION:STRING=${NUKE_VERSION}
endif

ifneq (${STOP_ON_WARNING},)
MY_CMAKE_FLAGS += -DSTOP_ON_WARNING:BOOL=${STOP_ON_WARNING}
endif

ifneq (${BUILD_SHARED_LIBS},)
MY_CMAKE_FLAGS += -DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}
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

ifneq (${OIIO_LIBNAME_SUFFIX},)
MY_CMAKE_FLAGS += -DOIIO_LIBNAME_SUFFIX:STRING=${OIIO_LIBNAME_SUFFIX}
endif

ifneq (${BUILD_OIIOUTIL_ONLY},)
MY_CMAKE_FLAGS += -DBUILD_OIIOUTIL_ONLY:BOOL=${BUILD_OIIOUTIL_ONLY}
endif

ifdef DEBUG
CMAKE_BUILD_TYPE=Debug
endif

ifdef PROFILE
CMAKE_BUILD_TYPE=RelWithDebInfo
endif

ifneq (${MYCC},)
MY_CMAKE_FLAGS += -DCMAKE_C_COMPILER:STRING="${MYCC}"
endif
ifneq (${MYCXX},)
MY_CMAKE_FLAGS += -DCMAKE_CXX_COMPILER:STRING="${MYCXX}"
endif

ifneq (${USE_CPP},)
MY_CMAKE_FLAGS += -DCMAKE_CXX_STANDARD=${USE_CPP}
endif

ifneq (${CMAKE_CXX_STANDARD},)
MY_CMAKE_FLAGS += -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
endif

ifneq (${USE_LIBCPLUSPLUS},)
MY_CMAKE_FLAGS += -DUSE_LIBCPLUSPLUS:BOOL=${USE_LIBCPLUSPLUS}
endif

ifneq (${GLIBCXX_USE_CXX11_ABI},)
MY_CMAKE_FLAGS += -DGLIBCXX_USE_CXX11_ABI=${GLIBCXX_USE_CXX11_ABI}
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

ifneq (${UNITY},)
  MY_CMAKE_FLAGS += -DCMAKE_UNITY_BUILD=ON -DCMAKE_UNITY_BUILD_MODE=${UNITY}
endif

ifeq (${CODECOV},1)
  CMAKE_BUILD_TYPE=Debug
  MY_CMAKE_FLAGS += -DCODECOV:BOOL=${CODECOV}
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

ifneq (${CLANG_FORMAT_INCLUDES},)
  MY_CMAKE_FLAGS += -DCLANG_FORMAT_INCLUDES:STRING=${CLANG_FORMAT_INCLUDES}
endif
ifneq (${CLANG_FORMAT_EXCLUDES},)
  MY_CMAKE_FLAGS += -DCLANG_FORMAT_EXCLUDES:STRING=${CLANG_FORMAT_EXCLUDES}
endif

ifneq (${BUILD_MISSING_DEPS},)
  MY_CMAKE_FLAGS += -DBUILD_MISSING_DEPS:BOOL=${BUILD_MISSING_DEPS}
endif


#$(info MY_CMAKE_FLAGS = ${MY_CMAKE_FLAGS})
#$(info MY_MAKE_FLAGS = ${MY_MAKE_FLAGS})

#########################################################################




#########################################################################
# Top-level documented targets

all: install

# 'make debug' is implemented via recursive make setting DEBUG
debug:
	${MAKE} DEBUG=1 --no-print-directory

# 'make profile' is implemented via recursive make setting PROFILE
profile:
	${MAKE} PROFILE=1 --no-print-directory

# 'make config' constructs the build directory and runs 'cmake' there,
# generating makefiles to build the project.  For speed, it only does this when
# ${build_dir}/Makefile doesn't already exist, in which case we rely on the
# cmake generated makefiles to regenerate themselves when necessary.
config:
	@ (if [ ! -e ${build_dir}/${BUILDSENTINEL} ] ; then \
		${CMAKE} -E make_directory ${build_dir} ; \
		cd ${build_dir} ; \
		${CMAKE} -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE} \
			 -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} \
			 ${MY_CMAKE_FLAGS} ${working_dir} ; \
	 fi)


# 'make build' does a basic build (after first setting it up)
build: config
	@ ( cd ${build_dir} ; \
	    ${CMAKE} --build . --config ${CMAKE_BUILD_TYPE} \
	  )

# 'make install' builds everthing and installs it in 'dist'.
# Suppress pointless output from docs installation.
install: build
	@ ( cd ${build_dir} ; \
	    ${CMAKE} --build . --target install --config ${CMAKE_BUILD_TYPE} | grep -v '^-- \(Installing\|Up-to-date\|Set runtime path\)' \
	  )

# 'make package' builds everything and then makes an installable package
# (platform dependent -- may be .tar.gz, .sh, .dmg, .rpm, .deb. .exe)
package: install
	@ ( cd ${build_dir} ; \
	    ${CMAKE} --build . --target package --config ${CMAKE_BUILD_TYPE} \
	  )

# 'make package_source' makes an installable source package
# (platform dependent -- may be .tar.gz, .sh, .dmg, .rpm, .deb. .exe)
package_source: install
	@ ( cd ${build_dir} ; \
	    ${CMAKE} --build . --target package_source --config ${CMAKE_BUILD_TYPE} \
	  )

# 'make clang-format' runs clang-format on all source files (if it's installed)
clang-format: config
	@ ( cd ${build_dir} ; \
	    ${CMAKE} --build . --target clang-format --config ${CMAKE_BUILD_TYPE} \
	  )


# DEPRECATED: 'make dist' is just a synonym for 'make install'
dist : install

TEST_FLAGS += --force-new-ctest-process --output-on-failure

# 'make test' does a full build and then runs all tests
test: build
	@ ${CMAKE} -E cmake_echo_color --switch=$(COLOR) --cyan "Running tests ${TEST_FLAGS}..."
	@ ( cd ${build_dir} ; \
	    PYTHONPATH=${PWD}/${build_dir}/lib/python/site-packages \
	    ctest -E broken ${TEST_FLAGS} \
	  )
	@ ( if [[ "${CODECOV}" == "1" ]] ; then \
	      cd ${build_dir} ; \
	      lcov -b . -d . -c -o cov.info ; \
	      lcov --remove cov.info "/usr*" -o cov.info ; \
	      lcov --remove cov.info "*/usr/incude" -o cov.info ; \
	      lcov --remove cov.info "/Library/Developer/*" -o cov.info ; \
	      lcov --remove cov.info "*/detail/pugixml/*" -o cov.info ; \
	      lcov --remove cov.info "*/detail/fmt/*" -o cov.info ; \
	      lcov --remove cov.info "*/detail/farmhash.h" -o cov.info ; \
	      lcov --remove cov.info "*/v1/*" -o cov.info ; \
	      lcov --remove cov.info "*/ext/robin-map/*" -o cov.info ; \
	      lcov --remove cov.info "*/kissfft.hh" -o cov.info ; \
	      lcov --remove cov.info "*/stb_sprintf.h" -o cov.info ; \
	      genhtml -o ../_coverage -t "Test coverage" --num-spaces 4 cov.info ; \
	  fi )

# 'make testall' does a full build and then runs all tests (even the ones
# that are expected to fail on some platforms)
testall: build
	${CMAKE} -E cmake_echo_color --switch=$(COLOR) --cyan "Running all tests ${TEST_FLAGS}..."
	( cd ${build_dir} ; PYTHONPATH=${PWD}/${build_dir}/lib/python/site-packages ctest ${TEST_FLAGS} )

# 'make clean' clears out the build directory for this platform
clean:
	${CMAKE} -E remove_directory ${build_dir}

# 'make realclean' clears out both build and dist directories for this platform
realclean: clean
	${CMAKE} -E remove_directory ${dist_dir}

# DEPRECATED: 'make nuke' blows away the build and dist areas for all platforms
nuke: realclean

doxygen:
	doxygen src/doc/Doxyfile

#########################################################################



# 'make help' prints important make targets
help:
	@echo "Targets:"
	@echo "  make              Build and install optimized binaries and libraries"
	@echo "  make install      Build and install optimized binaries and libraries"
	@echo "  make build        Build only (no install) optimized binaries and libraries"
	@echo "  make config       Just configure cmake, don't build"
	@echo "  make debug        Build and install unoptimized with symbols"
	@echo "  make profile      Build and install for profiling"
	@echo "  make clean        Remove the temporary files in ${build_dir}"
	@echo "  make realclean    Remove both ${build_dir} AND ${dist_dir}"
	@echo "  make test         Run tests"
	@echo "  make testall      Run all tests, even broken ones"
	@echo "  make clang-format Run clang-format on all the source files"
	@echo ""
	@echo "Helpful modifiers:"
	@echo "  C++ compiler and build process:"
	@echo "      VERBOSE=1                Show all compilation commands"
	@echo "      STOP_ON_WARNING=0        Do not stop building if compiler warns"
	@echo "      OPENIMAGEIO_SITE=xx      Use custom site build mods"
	@echo "      MYCC=xx MYCXX=yy         Use custom compilers"
	@echo "      CMAKE_CXX_STANDARD=14    C++ standard to build with (default is 14)"
	@echo "      USE_LIBCPLUSPLUS=1       For clang, use libc++"
	@echo "      GLIBCXX_USE_CXX11_ABI=1  For gcc, use the new string ABI"
	@echo "      EXTRA_CPP_ARGS=          Additional args to the C++ command"
	@echo "      USE_NINJA=1              Set up Ninja build (instead of make)"
	@echo "      USE_CCACHE=0             Disable ccache (even if available)"
	@echo "      UNITY=BATCH              Do a 'Unity' build (BATCH or GROUP or nothing)"
	@echo "      CODECOV=1                Enable code coverage tests"
	@echo "      SANITIZE=name1,...       Enable sanitizers (address, leak, thread)"
	@echo "      CLANG_TIDY=1             Run clang-tidy on all source (can be modified"
	@echo "                                  by CLANG_TIDY_ARGS=... and CLANG_TIDY_FIX=1"
	@echo "      CLANG_FORMAT_INCLUDES=... CLANG_FORMAT_EXCLUDES=..."
	@echo "                               Customize files for 'make clang-format'"
	@echo "  Linking and libraries:"
	@echo "      SOVERSION=nn             Include the specified major version number "
	@echo "                                  in the shared object metadata"
	@echo "      OIIO_LIBNAME_SUFFIX=name Optional name appended to library names"
	@echo "      BUILD_SHARED_LIBS=0      Build static library instead of shared"
	@echo "      LINKSTATIC=1             Link with static external libs when possible"
	@echo "  Dependency hints:"
	@echo "      For each dependeny Foo, defining ENABLE_Foo=0 disables it, even"
	@echo "      if found. And you can hint where to find it with Foo_ROOT=path"
	@echo "      Note that it is case sensitive! The list of package names is:"
	@echo "          DCMTK  FFmpeg  Freetype  GIF  JPEGTurbo"
	@echo "          LibRaw  OpenColorIO  OpenCV  OpenGL  OpenJpeg  OpenVDB"
	@echo "          PTex  R3DSDK  TBB  TIFF  Webp"
	@echo "  Finding and Using Dependencies:"
	@echo "      BOOST_ROOT=path          Custom Boost installation"
	@echo "      OPENEXR_ROOT=path        Custom OpenEXR installation"
	@echo "      ILMBASE_ROOT=path        Custom IlmBase installation"
	@echo "      USE_EXTERNAL_PUGIXML=1   Use the system PugiXML, not the one in OIIO"
	@echo "      USE_QT=0                 Skip anything that needs Qt"
	@echo "      USE_PYTHON=0             Don't build the Python binding"
	@echo "      PYTHON_VERSION=2.6       Specify the Python version"
	@echo "      USE_NUKE=0               Don't build Nuke plugins"
	@echo "      Nuke_ROOT=path           Custom Nuke installation"
	@echo "      NUKE_VERSION=ver         Custom Nuke version"
	@echo "  OIIO build-time options:"
	@echo "      INSTALL_PREFIX=path      Set installation prefix (default: ./${INSTALL_PREFIX})"
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


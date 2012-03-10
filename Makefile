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
INSTALLDIR       =${working_dir}

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
MY_CMAKE_FLAGS ?= -DSELF_CONTAINED_INSTALL_TREE:BOOL=TRUE

# Site-specific build instructions
ifndef OPENIMAGEIO_SITE
    OPENIMAGEIO_SITE := ${shell uname -n}
endif
$(info OPENIMAGEIO_SITE = ${OPENIMAGEIO_SITE})
ifneq (${shell echo ${OPENIMAGEIO_SITE} | grep imageworks},)
include ${working_dir}/site/spi/Makefile-bits
endif

# Set up variables holding the names of platform-dependent directories --
# set these after evaluating site-specific instructions
top_build_dir := build
build_dir     := ${top_build_dir}/${platform}${variant}
top_dist_dir  := dist
dist_dir      := ${top_dist_dir}/${platform}${variant}

$(info dist_dir = ${dist_dir})
$(info INSTALLDIR = ${INSTALLDIR})


VERBOSE := ${SHOWCOMMANDS}
ifneq (${VERBOSE},)
MY_MAKE_FLAGS += VERBOSE=${VERBOSE}
TEST_FLAGS += -V
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

ifneq (${USE_TBB},)
MY_CMAKE_FLAGS += -DUSE_TBB:BOOL=${USE_TBB}
endif

ifneq (${NOTHREADS},)
MY_CMAKE_FLAGS += -DNOTHREADS:BOOL=${NOTHREADS}
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

ifneq (${USE_FIELD3D},)
MY_CMAKE_FLAGS += -DUSE_FIELD3D:BOOL=${USE_FIELD3D}
endif

ifneq (${USE_OPENJPEG},)
MY_CMAKE_FLAGS += -DUSE_OPENJPEG:BOOL=${USE_OPENJPEG}
endif

ifneq (${USE_OCIO},)
MY_CMAKE_FLAGS += -DUSE_OCIO:BOOL=${USE_OCIO}
endif

ifneq (${BUILDSTATIC},)
MY_CMAKE_FLAGS += -DBUILDSTATIC:BOOL=${BUILDSTATIC}
endif

ifneq (${LINKSTATIC},)
MY_CMAKE_FLAGS += -DLINKSTATIC:BOOL=${LINKSTATIC}
endif

ifneq (${SOVERSION},)
MY_CMAKE_FLAGS += -DSOVERSION:STRING=${SOVERSION}
endif

ifdef DEBUG
MY_CMAKE_FLAGS += -DCMAKE_BUILD_TYPE:STRING=Debug
endif

ifdef PROFILE
MY_CMAKE_FLAGS += -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo -DCMAKE_CXX_FLAGS_RELWITHDEBINFO:STRING="-O2 -pg -g -DDEBUG" -DCMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO:STRING="-pg"
endif

ifneq (${MYCC},)
MY_CMAKE_FLAGS += -DCMAKE_C_COMPILER:STRING=${MYCC}
endif
ifneq (${MYCXX},)
MY_CMAKE_FLAGS += -DCMAKE_CXX_COMPILER:STRING=${MYCXX}
endif

ifneq (${TEST},)
TEST_FLAGS += -R ${TEST}
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
	@ (if [ ! -e ${build_dir}/Makefile ] ; then \
		cmake -E make_directory ${build_dir} ; \
		cd ${build_dir} ; \
		cmake -DCMAKE_INSTALL_PREFIX=${INSTALLDIR}/${dist_dir} \
			${MY_CMAKE_FLAGS} -DBOOST_ROOT=${BOOST_HOME} \
			../../src ; \
	 fi)

# 'make cmake' does a basic build (after first setting it up)
cmake: cmakesetup
	( cd ${build_dir} ; ${MAKE} ${MY_MAKE_FLAGS} )

# 'make cmakeinstall' builds everthing and installs it in 'dist'.
# Suppress pointless output from docs installation.
cmakeinstall: cmake
	( cd ${build_dir} ; ${MAKE} ${MY_MAKE_FLAGS} install | grep -v '^-- \(Installing\|Up-to-date\).*doc/html' )

# 'make dist' is just a synonym for 'make cmakeinstall'
dist : cmakeinstall

# 'make test' does a full build and then runs all tests
test: cmake
	cmake -E cmake_echo_color --switch=$(COLOR) --cyan "Running tests ${TEST_FLAGS}..."
	( cd ${build_dir} ; ctest --force-new-ctest-process ${TEST_FLAGS} -E broken )

# 'make testall' does a full build and then runs all tests (even the ones
# that are expected to fail on some platforms)
testall: cmake
	cmake -E cmake_echo_color --switch=$(COLOR) --cyan "Running all tests ${TEST_FLAGS}..."
	( cd ${build_dir} ; ctest --force-new-ctest-process ${TEST_FLAGS} )

# 'make package' builds everything and then makes an installable package 
# (platform dependent -- may be .tar.gz, .sh, .dmg, .rpm, .deb. .exe)
package: cmakeinstall
	( cd ${build_dir} ; ${MAKE} ${MY_MAKE_FLAGS} package )

# 'make package_source' makes an installable source package 
# (platform dependent -- may be .tar.gz, .sh, .dmg, .rpm, .deb. .exe)
package_source: cmakeinstall
	( cd ${build_dir} ; ${MAKE} ${MY_MAKE_FLAGS} package_source )

#clean: testclean
# 'make clean' clears out the build directory for this platform
clean:
	cmake -E remove_directory ${build_dir}

# 'make realclean' clears out both build and dist directories for this platform
realclean: clean
	cmake -E remove_directory ${dist_dir}

# 'make nuke' blows away the build and dist areas for all platforms
nuke:
	cmake -E remove_directory ${top_build_dir}
	cmake -E remove_directory ${top_dist_dir}

doxygen:
	doxygen src/doc/Doxyfile

#########################################################################



# 'make help' prints important make targets
help:
	@echo "Targets:"
	@echo "  make              Build optimized binaries and libraries in ${dist_dir},"
	@echo "                        temporary build files in ${build_dir}"
	@echo "  make debug        Build unoptimized with symbols in ${dist_dir}.debug,"
	@echo "                        temporary build files in ${build_dir}.debug"
	@echo "  make profile      Build for profiling in ${dist_dir}.profile,"
	@echo "                        temporary build files in ${build_dir}.profile"
	@echo "  make clean        Remove the temporary files in ${build_dir}"
	@echo "  make realclean    Remove both ${build_dir} AND ${dist_dir}"
	@echo "  make nuke         Remove ALL of build and dist (not just ${platform})"
	@echo "  make test         Run tests"
	@echo "  make testall      Run all tests, even broken ones"
	@echo "  make doxygen      Build the Doxygen docs in ${top_build_dir}/doxygen"
	@echo ""
	@echo "Helpful modifiers:"
	@echo "  make VERBOSE=1 ...          Show all compilation commands"
	@echo "  make SOVERSION=nn ...       Include the specifed major version number "
	@echo "                                in the shared object metadata"
	@echo "  make NAMESPACE=name         Wrap everything in another namespace"
	@echo "  make EMBEDPLUGINS=0 ...     Don't compile the plugins into libOpenImageIO"
	@echo "  make MYCC=xx MYCXX=yy ...   Use custom compilers"
	@echo "  make USE_QT=0 ...           Skip anything that needs Qt"
	@echo "  make USE_OPENGL=0 ...       Skip anything that needs OpenGL"
	@echo "  make FORCE_OPENGL_1=1 ...   Force iv to use OpenGL's fixed pipeline"
	@echo "  make USE_TBB=0 ...          Don't use TBB"
	@echo "  make USE_PYTHON=0 ...       Don't build the Python binding"
	@echo "  make PYTHON_VERSION=2.6 ... Specify the Python version"
	@echo "  make USE_FIELD3D=0 ...      Don't build the Field3D plugin"
	@echo "  make USE_OPENJPEG=0 ...     Don't build the JPEG-2000 plugin"
	@echo "  make USE_OCIO=0 ...         Don't use OpenColorIO even if found"
	@echo "  make BUILDSTATIC=1 ...      Build static library instead of shared"
	@echo "  make LINKSTATIC=1 ...       Link with static external libraries when possible"
	@echo ""


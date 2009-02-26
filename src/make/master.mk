# License and copyright goes here

#########################################################################
# global.mk
#
# This is the master makefile.
# Here we put all the top-level make targets, platform-independent
# rules, etc.
#
# Targets we support:
#
#   all (default) - build optimized application in build/<platform>, and 
#                   installers here, if applicable.
#
#   debug - build unoptimized application with all debug symbols
#           in build/platform.debug
#
#   profile - build optimized and profiled application in 
#             build/platform.profile
#
#   clean - remove all temporary files used to compile (generally
#           everything in build/platform)
#
#   realclean - remove both the build and dist for this platform
#
#   nuke - remove everything but the bare source -- including builds
#          and dists for all platforms
#
#########################################################################



# Phony targets
.PHONY: all debug profile clean realclean nuke \
	doxygen

# All the suffixes we see and have rules for
# do we need these?
#.SUFFIXES: .cpp 


# Set up variables holding the names of source directories
working_dir	:= ${shell pwd}
top_dir         := .
top_src_dir     := ${top_dir}/src
src_dir         := ${top_dir}/src
src_include_dir := ${top_src_dir}/include
src_make_dir    := ${top_src_dir}/make



# Figure out which architecture we're on and include any
# platform-specific makefiles
include ${src_make_dir}/platform.mk



# Presence of make variables DEBUG and PROFILE cause us to make special
# builds, which we put in their own areas.
ifdef DEBUG
    variant +=.debug
    CFLAGS += -DDEBUG
endif
ifdef PROFILE
    variant +=.profile
    CFLAGS += -DNDEBUG
endif


# Set up variables holding the names of platform-dependent directories
top_build_dir := ${top_dir}/build
build_dir     := ${top_build_dir}/${platform}${variant}
build_obj_dir := ${build_dir}/obj

top_dist_dir := ${top_dir}/dist
dist_dir     := ${top_dist_dir}/${platform}${variant}



# Census of the directories containing things to build, the local makefiles

# All directories containing source of binaries or libraries
#src_dirs := ${wildcard ${src_dir}/*}
#$(info src_dirs = "${src_dirs}")

# List of all local module.mk files for binaries
#all_makefiles := ${foreach f,${src_dirs},${wildcard ${f}/module.mk}}
all_makefiles := ${wildcard src/*/module.mk}
#$(info all_makefiles = "${makefiles}")

# List of all tests to run, by default testsuite/tests/*, but project.mk
# may amend this
all_tests := ${wildcard testsuite/tests/*}



# Making dist
build_dirs := bin lib include doc

# Directories to create in the distribution
dist_dirs := bin lib include doc

# default place where includes go in the dist
dist_include_dir := include



# Include the file containing all project-specific info
include ${src_make_dir}/project.mk



#########################################################################
# Include all per-module makefiles

# Initialize variables that the individual module.mk's will append to
ALL_SRC :=
ALL_BINS :=
ALL_LIBS :=
ALL_SHLIBS :=
ALL_TESTS :=
ALL_DEPS :=
ALL_BUILD_DIRS :=

#
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

clean: testclean
	${RM_ALL} ${build_dir}

realclean: clean
	${RM_ALL} ${dist_dir}

nuke:
	${RM_ALL} ${top_build_dir} ${top_dist_dir}

doxygen:
	doxygen src/doc/Doxyfile

test : ${all_tests}
	@ echo "Running test suite..."
	@ for f in ${all_tests} ; do \
	    ( cd $$f ; \
	      echo "$$f " ; \
	      PATH=../../../${build_dir}/bin:${PATH} \
	      LD_LIBRARY_PATH=../../../${build_dir}/lib:${LD_LIBRARY_PATH} \
	      DYLD_LIBRARY_PATH=../../../${build_dir}/lib:${DYLD_LIBRARY_PATH} \
	      ./run.py ; \
	    ) \
	done

testclean : ${all_tests}
	@ for f in ${all_tests} ; do \
	    ( cd $$f ; \
	      echo "Cleaning test $$f " ; \
	      ./run.py -c ; \
	    ) \
	done

# end top level targets
#########################################################################



#########################################################################
# Compilation rules

#$(info RULE build_obj_dir=${build_obj_dir}    src_bin_dir=${src_bin_dir})

# Old rule for compiling C++ files.  This has moved into bin.mk, lib.mk,
# etc., since it needs to be defined in that context.
#${build_obj_dir}/%${OEXT}: ${src_dir}/%.cpp
#	@ echo "Compiling $@ ..."
#	${CXX} ${CFLAGS} ${PROJECT_EXTRA_CXX} ${DASHC} $< ${DASHO}$@

# Rule for using Qt's "moc" preprocessor
moc_%.cpp: %.h
	@ echo "  Running 'moc' on $@ ..."
	@ $(QT_MOC) $< -o $@

# end compilation rules
#########################################################################




# FIXME
# This flag is set in the command module.mk files when
# the command is no longer used.  It is defined here so
# that it doesn't evaluate to the empty string and disable
# all commands that don't set it at all!
DO_NOT_BUILD := disabled

# Include the module.mk for each source directory
include ${all_makefiles}



#########################################################################
# Internal targets

build: make_build_dirs ${ALL_DEPS} build_libs build_bins build_docs

# Target to create all build directories
make_build_dirs:
	@ for f in ${build_dirs}; do ${MKDIR} ${build_dir}/$$f; done

build_libs: ${ALL_LIBS}
#	@ echo "ALL_LIBS = ${ALL_LIBS}"

build_bins: ${ALL_BINS}
#	@ echo "ALL_BINS = ${ALL_BINS}"

build_docs:

dist: build copy_dist_bins copy_dist_libs copy_dist_includes \
	copy_dist_extra_libs copy_dist_docs copy_dist_root

make_dist_dirs:
	@ for f in ${dist_dirs}; do ${MKDIR} ${dist_dir}/$$f; done

copy_dist_bins: make_dist_dirs
#	@ echo "Copying dist_bins = ${dist_bins}"
	@ for f in ${dist_bins}; do \
	    ${CP} ${build_dir}/bin/$$f ${dist_dir}/bin; \
	    ${CHMOD_RX} ${dist_dir}/bin/$$f ; \
	  done

copy_dist_libs: make_dist_dirs
#	@ echo "Copying dist_libs = ${dist_libs}"
	@ for f in ${dist_libs}; do \
	    ${CP} ${build_dir}/lib/$$f ${dist_dir}/lib; \
	    ${CHMOD_RO} ${dist_dir}/lib/$$f ; \
	  done

copy_dist_includes: make_dist_dirs
#	@ echo "Copying dist_includes = ${dist_includes}"
	@ ${MKDIR} ${dist_dir}/${dist_include_dir}
	@ for f in ${dist_includes}; do \
	    ${CP} ${src_include_dir}/$$f ${dist_dir}/${dist_include_dir} ; \
	    ${CHMOD_RO} ${dist_dir}/${dist_include_dir}/$$f ; \
	  done

copy_dist_extra_libs: make_dist_dirs
#	@ echo "Copying dist_extra_libs = ${dist_extra_libs}"
	@ for f in ${dist_extra_libs}; do \
	    ${CP} $$f ${dist_dir}/lib; \
	  done

copy_dist_docs: make_dist_dirs
#	@ echo "Copying dist_docs = ${dist_docs}"
	@ for f in ${dist_docs}; do \
	    ${CPR} $$f ${dist_dir}/doc; \
	  done

copy_dist_root: make_dist_dirs
#	@ echo "Copying dist_root = ${dist_root}"
	@ for f in ${dist_root}; do \
	    ${CPR} $$f ${dist_dir}; \
	  done


# end internal targets
#########################################################################



# Include all the dependency files
ifndef NODEP
-include ${ALL_DEPS}
endif

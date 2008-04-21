# License and copyright goes here
#foo

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
#           everything in build/platform/obj/)
#
#   realclean - remove everything but the bare source (everything in
#               build/)
#
#########################################################################



# Phony targets
.PHONY: all debug profile clean realclean

# All the suffixes we see and have rules for
# do we need these?
#.SUFFIXES: .cpp 


# Set up variables holding the names of source directories
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
variant :=
ifdef DEBUG
    variant += .debug
endif
ifdef PROFILE
    variant += .profile
endif


# Set up variables holding the names of platform-dependent directories
top_build_dir := ${top_dir}/build
build_dir     := ${top_build_dir}/${platform}${variant}
build_obj_dir := ${build_dir}/obj




# Census of the directories containing things to build, the local makefiles

# All directories containing source of binaries or libraries
#src_dirs := ${wildcard ${src_dir}/*}
#$(info src_dirs = "${src_dirs}")

# List of all local module.mk files for binaries
#all_makefiles := ${foreach f,${src_dirs},${wildcard ${f}/module.mk}}
all_makefiles := ${wildcard src/*/module.mk}
$(info all_makefiles = "${makefiles}")



# Making dist

# Directories to create in the distribution
dist_dirs := bin lib include doc

# Libraries we put in the distribution
#dist_libs = 
#lib/alib${LIBEXT} lib/blib${SHLIBEXT}

# Binaries we put in the distribution
#dist_bins := bin/c${BINEXT}


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

all: build


# 'make debug' is implemented via recursive make setting DEBUG
debug:
	${MAKE} DEBUG=true --no-print-directory

# 'make profile' is implemented via recursive make setting PROFILE
profile:
	${MAKE} PROFILE=true --no-print-directory

clean:
	${RM_ALL} ${build_dir}/obj

realclean:
	${RM_ALL} ${top_build_dir}

# end top level targets
#########################################################################



#########################################################################
# Compilation rules

$(info RULE build_obj_dir=${build_obj_dir}    src_bin_dir=${src_bin_dir})

${build_obj_dir}/%${OEXT}: ${src_dir}/%.cpp
	@ echo "Compiling $@ ..."
	${CXX} ${CFLAGS} ${DASHC} $< ${DASHO}$@

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

strip:
ifndef DEBUG
	@ echo -n "Stripping binaries in ${dist_dir}/bin ..."
	@ ${MAKE_WRITABLE} ${build_dir}/bin/*${BINEXT}
	@ ${STRIP_BINARY} ${build_dir}/bin/*${BINEXT}
	@ ${MAKE_UNWRITABLE} ${build_dir}/bin/*${BINEXT}
endif

build: make_build_dirs build_bins build_libs build_includes \
		build_docs

# Target to create all dest directories
make_build_dirs:
	@ for f in ${dist_dirs}; do ${MKDIR} ${build_dir}/$$f; done

build_libs: ${ALL_LIBS}
	@ echo "ALL_LIBS = ${ALL_LIBS}"

build_bins: ${ALL_BINS}
	@ echo "ALL_BINS = ${ALL_BINS}"

build_includes:

build_docs:

dist: strip

# end internal targets
#########################################################################



# Include all the dependency files
ifndef NODEP
-include ${ALLDEPS}
endif

# License and copyright goes here


# This file contains the rules for building a binary executable out
# of a single local source directory.
#
# This is included directly from the module.mk of that local directory.


#${info Reading bin.mk for ${local_name}}

# Name for this binary
name := ${notdir ${local_name}}

# Full path to the directory containing the local source
${name}_src_dir := ${src_dir}/${name}
#${info bin.mk ${name} ${name}_src_dir = ${${name}_src_dir}}

# List of all source files, with full paths
${name}_srcs := ${foreach f,${local_src},${${name}_src_dir}/${f}}
#${info bin.mk ${name} ${name}_srcs = ${${name}_srcs}}

# Directory where we're going to build the object files for this binary
${name}_obj_dir := ${build_obj_dir}/${name}
#${info bin.mk ${name} ${name}_obj_dir = ${${name}_obj_dir}}

# List of all obj files we need to generate for this binary
${name}_objs := ${patsubst %.cpp,%${OEXT},${foreach f,${local_src},${${name}_obj_dir}/${f}}}
${name}_objs += ${local_extra_objs}
#${info bin.mk ${name} ${name}_objs = ${${name}_objs}}

# Full path and name of the executable
${name}_bin := ${build_dir}/bin/${name}${BINEXT}
#${info bin.mk ${name} ${name}_bin = ${${name}_bin}}

# Libs we need to build
${name}_needed_libs := ${foreach f,${local_libs},${build_dir}/lib/${f}${LIBEXT}}
${name}_needed_libs += ${foreach f,${local_shlibs},${build_dir}/lib/${f}${SHLIBEXT}}
#${info bin.mk ${name} ${${name}_bin} needs libs ${${name}_needed_libs}}

# Libs to link against
${name}_linked_libs := ${foreach f,${local_libs},${build_dir}/lib/${f}${LIBEXT}}
${name}_linked_libs += ${patsubst lib%,-l%,${foreach f,${local_shlibs},${f}}}
#${name}_linked_libs +=${foreach f,${local_shlibs},${f}${SHLIBEXT}}

# Local linking arguments
${name}_ldflags := ${local_ldflags}
#${info bin.mk ${name} ${name}_ldflags = ${${name}_ldflags}}

# Dependency file is build/<platform>/obj/<name>.d
${name}_depfile := ${${name}_obj_dir}/${name}.d
#${info ${name} dep file = ${${name}_depfile}}


ifneq (${DO_NOT_BUILD},${name})

# Take all the source modules in 
ALL_SRC += ${${name}_srcs}
ALL_BINS += ${${name}_bin}
ALL_DEPS += ${${name}_depfile}
ALL_BUILD_DIRS += ${${name}_obj_dir}
#${info In bin.mk, now ALL_DEPS = ${ALL_DEPS}}
#${info In bin.mk, now ALL_BUILD_DIRS = ${ALL_BUILD_DIRS}}

endif



# Action to build the binary
${${name}_bin}: ${${name}_srcs} ${${name}_depfile} ${${name}_objs} ${${name}_needed_libs}
	@ echo "Building binary $@ ..."
ifeq (${SHOWCOMMANDS},)
	@ ${LD} ${LDFLAGS} ${BINOUT}$@ ${${notdir ${basename $@}}_objs} ${LD_LIBPATH}${build_dir}/lib ${${notdir ${basename $@}}_linked_libs} ${${basename ${notdir $@}}_ldflags} ${LINK_OTHER}
else
	${LD} ${LDFLAGS} ${BINOUT}$@ ${${notdir ${basename $@}}_objs} ${LD_LIBPATH}${build_dir}/lib ${${notdir ${basename $@}}_linked_libs} ${${basename ${notdir $@}}_ldflags} ${LINK_OTHER}
endif
ifndef DEBUG
	@ ${STRIP_BINARY} $@
endif

# Action to build the object files
${${name}_obj_dir}/%${OEXT}: ${${name}_src_dir}/%.cpp
	@ echo "  Compiling $@ ..."
ifeq (${SHOWCOMMANDS},)
	@ ${CXX} ${CFLAGS} ${CINCL}${${name}_src_dir} ${PROJECT_EXTRA_CXX} ${DASHC} $< ${DASHO}$@
else
	${CXX} ${CFLAGS} ${CINCL}${${name}_src_dir} ${PROJECT_EXTRA_CXX} ${DASHC} $< ${DASHO}$@
endif

# Action to build the dependency if any of the src files change
${${name}_depfile}: ${${name}_srcs}
	@ echo "Building bin dependency $@ from $^ ..."
	@ ${MKDIR} ${build_dir} ${build_dir}/obj ${ALL_BUILD_DIRS}
	@ ${MAKEDEPEND} -f- -- ${CFLAGS} ${CINCL}${${notdir ${basename $@}}_src_dir} -- ${${notdir ${basename $@}}_srcs} 2>/dev/null \
		| ${SED} -e 's%^${${notdir ${basename $@}}_src_dir}%${${notdir ${basename $@}}_obj_dir}%g' \
		> ${${notdir ${basename $@}}_depfile}


local_name :=
local_src :=
local_extra_objs :=
local_libs :=
local_shlibs :=
local_ldflags :=

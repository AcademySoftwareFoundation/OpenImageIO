# License and copyright goes here


# This file contains the rules for building a static library out
# of a single local source directory.
#
# This is included directly from the module.mk of that local directory.


#${info Reading lib.mk for ${local_name}}

# Name for this library
name := ${notdir ${local_name}}

# Full path to the directory containing the local source
${name}_src_dir := ${src_dir}/${name}
#${info lib.mk ${name} ${name}_src_dir = ${${name}_src_dir}}

# List of all source files, with full paths
${name}_srcs := ${foreach f,${local_src},${${name}_src_dir}/${f}}
#${info lib.mk ${name} ${name}_srcs = ${${name}_srcs}}

# Directory where we're going to build the object files for this library
${name}_obj_dir := ${build_obj_dir}/${name}
#${info lib.mk ${name} ${name}_obj_dir = ${${name}_obj_dir}}

# List of all obj files we need to generate for this library
${name}_objs := ${patsubst %.cpp,%${OEXT},${foreach f,${local_src},${${name}_obj_dir}/${f}}}
${name}_objs += ${local_extra_objs}
#${info lib.mk ${name} ${name}_objs = ${${name}_objs}}

# Full path and name of the library
${name}_lib := ${build_dir}/lib/${name}${LIBEXT}
#${info lib.mk ${name} ${name}_lib = ${${name}_lib}}

# Dependency file is build/<platform>/obj/<name>.d
${name}_depfile := ${${name}_obj_dir}/${name}.d
#${info ${name} dep file = ${${name}_depfile}}


ifneq (${DO_NOT_BUILD},${name})

# Take all the source modules in 
ALL_SRC += ${${name}_srcs}
ALL_LIBS += ${${name}_lib}
ALL_DEPS += ${${name}_depfile}
ALL_BUILD_DIRS += ${${name}_obj_dir}

#${info In lib.mk, now ALL_DEPS = ${ALL_DEPS}}
#${info In lib.mk, now ALL_BUILD_DIRS = ${ALL_BUILD_DIRS}}

endif



# Action to build the library
${${name}_lib}: ${${name}_objs} ${${name}_depfile}
	@ echo "Building library ${${name}_lib} $@ ..."
ifeq (${SHOWCOMMANDS},)
	@ ${AR} ${AROUT}$@ ${${notdir ${basename $@}}_objs} 
else
	${AR} ${AROUT}$@ ${${notdir ${basename $@}}_objs} 
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
	@ echo "Building lib dependency $@ ..."
	@ ${MKDIR} ${build_dir} ${build_dir}/obj ${ALL_BUILD_DIRS}
	@ ${MAKEDEPEND} -f- -- ${CFLAGS} ${CINCL}${${notdir ${basename $@}}_src_dir} -- ${${notdir ${basename $@}}_srcs} 2>/dev/null \
		| ${SED} -e 's%^${${notdir ${basename $@}}_src_dir}%${${notdir ${basename $@}}_obj_dir}%g' \
		> ${${notdir ${basename $@}}_depfile}


local_name :=
local_src :=
local_extra_objs :=
local_libs :=
local_shlibs :=

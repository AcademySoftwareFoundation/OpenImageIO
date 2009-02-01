# License and copyright goes here


# This file contains the rules for building a third party package from
# source contained in a .tar.gz file.
#
# This is included directly from the module.mk of that local directory.
# 
# The module.mk file should set the following variables:
#   local_name          base name of the package
#   local_src           package tar filename
#   local_config        any special flags to give ./configure
#   local_make_extras   any special flags for make when building the package
#   local_patches       any commands to run after untar but before configure

${info Reading srcpackage.mk for ${local_name}}

# Name for this library
name := ${notdir ${local_name}}

# Full path to the directory containing the local source
${name}_src_dir := ${src_dir}/${name}
#${info srcpackage.mk ${name} ${name}_src_dir = ${${name}_src_dir}}

# List of all source files, with full paths
${name}_srcs := ${foreach f,${local_src},${${name}_src_dir}/${f}}
#${info srcpackage.mk ${name} ${name}_srcs = ${${name}_srcs}}

# Directory where we're going to build the object files for this library
${name}_obj_dir := ${build_obj_dir}/${name}
#${name}_obj_dir := ${build_dir}/${name}
#${info srcpackage.mk ${name} ${name}_obj_dir = ${${name}_obj_dir}}

# Config flags
${name}_config := ${local_config}
#${info srcpackage.mk ${name} ${name}_config = ${${name}_config}}

# Patch commands
ifneq (${local_patches},)
${name}_patches := ${local_patches}
else
${name}_patches := echo "no patches"
endif

# Extra make flags flags
${name}_make_extras := ${local_make_extras}
#${info srcpackage.mk ${name} ${name}_make_extras = ${${name}_make_extras}}

# Build dependency file is build/<platform>/obj/<name>.d
${name}_depfile := ${${name}_obj_dir}/${name}.d
#${info ${name} dep file = ${${name}_depfile}}

# Dist dependency file is dist/<platform>/<name>/<name>.d
${name}_dist_depfile := ${dist_dir}/${name}.d
#${info ${name} dist dep file = ${${name}_dist_depfile}}

ALL_DEPS += ${${name}_depfile} ${${name}_dist_depfile}
ALL_BUILD_DIRS += ${${name}_obj_dir}


# Rule to make the build dependency -- which, as a side effect, untars
# the package in the 'obj' area and builds it there.
${${name}_depfile}: ${${name}_srcs}
	@ echo "Building $@ ..."
	${MKDIR} ${build_dir} ${build_obj_dir} ${${notdir ${basename $@}}_obj_dir} ${ALL_BUILD_DIRS}
	tar -x -z -f ${${notdir ${basename $@}}_srcs} \
	        -C ${build_obj_dir}
	${${notdir ${basename $@}}_patches}
	(cd ${${notdir ${basename $@}}_obj_dir} ; if [ -e ./configure ] ; then ./configure --prefix=${working_dir}/${build_obj_dir}/${notdir ${basename $@}} ${${notdir ${basename $@}}_config} ; fi ; make ${${notdir ${basename $@}}_make_extras} )
	touch $@


# Make a target that does the build of just this package
${local_name} : ${${local_name}_dist_depfile}


# The file to copy the build to the dist area, i.e. the target of
# ${name}_dist_depfile, is given back in the module.mk file that called
# this file.  That's because it's totally custom for each package.
#
# But as an example, it might look something like this:
#
# ${${name}_dist_depfile}: ${${name}_depfile}
#	${MKDIR} ${dist_dir}
#	${MKDIR} ${dist_dir}/include
#	${MKDIR} ${dist_dir}/include/${notdir ${basename $@}}
#	${MKDIR} ${dist_dir}/lib
#	${MKDIR} ${dist_dir}/lib/${notdir ${basename $@}}
#	${CP} ${${notdir ${basename $@}}_obj_dir}/include/*.h ${dist_dir}/include/${notdir ${basename $@}}
#	${CP} ${${notdir ${basename $@}}_obj_dir}/lib/*${LIBEXT} ${dist_dir}/include/${notdir ${basename $@}}
#


local_name := 
local_src := 
local_config := 
local_make_extras := 
local_patches :=

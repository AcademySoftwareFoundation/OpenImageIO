# License and copyright goes here

ifneq (${USE_GTEST},0)

# Name of the binary or library whose source is in this directory.
# Do NOT include .exe or any other suffix.

# needs to match directory name
# FIXME(nemec): can we make this automatic?
local_name := test_libOpenImageIO

# Name of all source files in this directory
local_src := test_imageio.cpp

# Extra static libs needed to compile this binary (leave blank if this
# module is not for a binary executable)
local_libs := 

# Extra shared libs needed to compile this binary (leave blank if this
# module is not for a binary executable)
local_shlibs := libOpenImageIO

# ld flags needed for this module
local_ldflags := ${LINK_BOOST} ${LINK_ILMBASE} ${LINK_GTEST}



include ${src_make_dir}/bin.mk


endif

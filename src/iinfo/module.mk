# License and copyright goes here

# Name of the binary or library whose source is in this directory.
# Do NOT include .exe or any other suffix.
local_name := iinfo

# Name of all source files in this directory
local_src := iinfo.cpp

# Extra static libs needed to compile this binary (leave blank if this
# module is not for a binary executable)
local_libs := 

# Extra shared libs needed to compile this binary (leave blank if this
# module is not for a binary executable)
local_shlibs := libimageio libutil

# ld flags needed for this library
local_ldflags := ${LINK_BOOST} ${LINK_ILMBASE}



include ${src_make_dir}/bin.mk

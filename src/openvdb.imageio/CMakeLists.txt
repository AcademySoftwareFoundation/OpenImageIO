# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

if (OpenVDB_FOUND)
    add_oiio_plugin (openvdbinput.cpp
                     INCLUDE_DIRS  ${OPENVDB_INCLUDES} ${TBB_INCLUDE_DIRS}
                     LINK_LIBRARIES  ${OPENVDB_LIBRARIES}
                                     $<TARGET_NAME_IF_EXISTS:TBB::tbb>)
endif()

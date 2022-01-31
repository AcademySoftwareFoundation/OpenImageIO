# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio

if (OpenVDB_FOUND)
    add_oiio_plugin (openvdbinput.cpp
                     INCLUDE_DIRS ${OPENVDB_INCLUDES} ${TBB_INCLUDE_DIRS}
                     LINK_LIBRARIES ${OPENVDB_LIBRARIES}
                                    ${TBB_tbb_LIBRARY}
                                    $<TARGET_NAME_IF_EXISTS:TBB::tbb>
    )
endif()

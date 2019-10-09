# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

if (OPENVDB_FOUND)
    add_oiio_plugin (openvdbinput.cpp
                     INCLUDE_DIRS ${OPENVDB_INCLUDE_DIRS} ${TBB_INCLUDE_DIRS}
                     LINK_LIBRARIES ${OPENVDB_LIBRARIES} ${TBB_LIBRARIES} ${BOOST_LIBRARIES})

    if (CMAKE_COMPILER_IS_GNUCC AND ${GCC_VERSION} VERSION_GREATER_EQUAL 6.0
        AND ${GCC_VERSION} VERSION_LESS 7.0)
        set_source_files_properties (openvdbinput.cpp
                                     PROPERTIES COMPILE_FLAGS -Wno-error=strict-overflow)
    endif ()

endif()

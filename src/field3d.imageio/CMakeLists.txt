# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

if (Field3D_FOUND)
    # checked_find_package (HDF5)
    find_library (SZIP_LIBRARY NAMES sz)
    if (NOT SZIP_LIBRARY)
        set (SZIP_LIBRARY "")
    endif ()
    add_oiio_plugin (field3dinput.cpp field3doutput.cpp
                     INCLUDE_DIRS ${FIELD3D_INCLUDES}
                     LINK_LIBRARIES ${FIELD3D_LIBRARIES}
                                    # ${HDF5_LIBRARIES}
                                    ${SZIP_LIBRARY})
endif()

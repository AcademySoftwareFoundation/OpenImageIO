# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

if (DCMTK_FOUND)
    add_oiio_plugin (dicominput.cpp
                     INCLUDE_DIRS ${DCMTK_INCLUDE_DIR}
                     LINK_LIBRARIES ${DCMTK_LIBRARIES}
                     DEFINITIONS "-DUSE_DCMTK=1")
else ()
    message (WARNING "DICOM plugin will not be built, no DCMTK")
endif ()

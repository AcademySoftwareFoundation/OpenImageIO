# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

if (DCMTK_FOUND)
    add_oiio_plugin (dicominput.cpp
                     LINK_LIBRARIES DCMTK::DCMTK
                     DEFINITIONS "USE_DCMTK=1")
else ()
    message (WARNING "DICOM plugin will not be built, no DCMTK")
endif ()

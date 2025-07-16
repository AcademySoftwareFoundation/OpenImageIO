# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

if (JXL_FOUND)
    add_oiio_plugin (jxlinput.cpp jxloutput.cpp
                     INCLUDE_DIRS ${JXL_INCLUDES}
                     LINK_LIBRARIES ${JXL_LIBRARIES}
                     DEFINITIONS "USE_JXL")
else()
    message (WARNING "JPEG XL plugin will not be built")
endif()

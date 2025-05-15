# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

if (OPENJPH_FOUND)
    add_oiio_plugin (jphoutput.cpp
                     INCLUDE_DIRS ${OPENJPH_INCLUDES}
                     LINK_LIBRARIES ${OPENJPH_LIBRARIES}
                     DEFINITIONS "USE_OPENJPH")
else()
    message (WARNING "HTJpeg-2000 JPH plugin will not be built")
endif()

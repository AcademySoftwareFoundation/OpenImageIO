# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

if (APV_FOUND)
    add_oiio_plugin (apvinput.cpp apvoutput.cpp
                     INCLUDE_DIRS ${APV_INCLUDES}
                     LINK_LIBRARIES ${APV_LIBRARIES}
                     DEFINITIONS "USE_APV" ${APV_DEFINITIONS})
else ()
    message (WARNING "APV plugin will not be built")
endif ()

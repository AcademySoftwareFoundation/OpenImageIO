# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

if (WebP_FOUND)
    add_oiio_plugin (webpinput.cpp webpoutput.cpp
                     LINK_LIBRARIES WebP::webp WebP::webpdemux
                     DEFINITIONS "USE_WEBP=1")
else ()
    message (STATUS "WebP plugin will not be built")
endif()
